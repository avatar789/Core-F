/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TransmogrificationMgr.h"
#include "Bag.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "StringFormat.h"
#include <sstream>

TransmogrificationMgr::TransmogrificationMgr() { }

TransmogrificationMgr::~TransmogrificationMgr() { }

TransmogrificationMgr* TransmogrificationMgr::instance()
{
    static TransmogrificationMgr instance;
    return &instance;
}

void TransmogrificationMgr::LoadFromDB()
{
    _transmogrificationStore.clear();

    QueryResult result = CharacterDatabase.Query("SELECT item, transEntry FROM item_transmogrification");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 item = fields[0].Get<uint32>();
        uint32 transEntry = fields[1].Get<uint32>();
        _transmogrificationStore.emplace(item, transEntry);
    } while (result->NextRow());
}

uint32 TransmogrificationMgr::GetItemTransmogrification(uint64 guid) const
{
    TransmogrificationContainer::const_iterator itr = _transmogrificationStore.find(guid);
    if (itr != _transmogrificationStore.end())
        return itr->second;

    return 0;
}

void TransmogrificationMgr::UpdateItemTransmogrification(uint64 guid, uint32 transEntry)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_TRANSMOGRIFICATION_INFO);
    stmt->SetData(0, uint32(guid));
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_TRANSMOGRIFICATION_INFO);
    stmt->SetData(0, uint32(guid));
    stmt->SetData(1, transEntry);
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);

    _transmogrificationStore[guid] = transEntry;
}

void TransmogrificationMgr::RemoveItemTransmogrification(uint64 guid)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_TRANSMOGRIFICATION_INFO);
    stmt->SetData(0, uint32(guid));
    CharacterDatabase.Execute(stmt);

    _transmogrificationStore.erase(guid);
}

void TransmogrificationMgr::RemoveAllTransmogrificationByEntry(Player* player, uint32 entry)
{
    if (!player || player->GetItemCount(entry, true) > 1)
        return;

    auto transmogrifyRemover = [this, entry](Item* item)
    {
        if (GetItemTransmogrification(item->GetGUID().GetCounter()) == entry)
        {
            RemoveItemTransmogrification(item->GetGUID().GetCounter());
            UpdateItem(item);
        }
    };

    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            transmogrifyRemover(item);

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* bag = player->GetBagByPos(i))
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                if (Item* item = bag->GetItemByPos(j))
                    transmogrifyRemover(item);

    for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_BAG_END; ++i)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            transmogrifyRemover(item);

    for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* bag = player->GetBagByPos(i))
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                if (Item* item = bag->GetItemByPos(j))
                    transmogrifyRemover(item);
}

void TransmogrificationMgr::SendTransmogrificationMenuOpenTo(Player* player, Creature* creature)
{
    if (!player || !creature || !creature->IsTransmogrifier())
        return;

    player->SetCurrentTransmogrifier(creature->GetGUID().GetRawValue());
    player->SendAddonMessage(Acore::StringFormat("ASMSG_TRANSMOGRIFICATION_MENU_OPEN\t{}",
        GenerateTransmogrificationInfoFor(player)));
}

void TransmogrificationMgr::SendTransmogrificationMenuCloseTo(Player* player)
{
    if (!player || !player->GetCurrentTransmogrifier())
        return;

    player->SetCurrentTransmogrifier(0);
    player->SendAddonMessage("ASMSG_TRANSMOGRIFICATION_MENU_CLOSE");
}

void TransmogrificationMgr::HandleTransmogrificationPrepareRequestFrom(Player* player, uint8 pos, uint32 transEntry)
{
    if (!player)
        return;

    uint32 result = TRANSMOGRIFICATION_ERROR_OK;
    uint32 cost = 0;

    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, pos);
    ItemTemplate const* trans = sObjectMgr->GetItemTemplate(transEntry);
    if (!item || !trans)
        result = TRANSMOGRIFICATION_ERROR_DONT_REPORT;
    else
        result = CanBeTransmogrifiedBy(player, item, trans);

    if (result == TRANSMOGRIFICATION_ERROR_OK)
        cost = GetPriceForItem(item->GetTemplate());

    player->SendAddonMessage(Acore::StringFormat("ASMSG_TRANSMOGRIFICATION_PREPARE_RESPONSE\t{}:{}:{}:{}",
        pos + 1, transEntry, result, cost));
}

void TransmogrificationMgr::HandleTransmogrificationRemoveRequestFrom(Player* player, uint8 pos)
{
    if (!player)
        return;

    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, pos);
    if (!item)
        return;

    RemoveItemTransmogrification(item->GetGUID().GetCounter());
    UpdateItem(item);

    player->SendAddonMessage("ASMSG_TRANSMOGRIFICATION_REMOVE_RESPONSE");
}

void TransmogrificationMgr::HandleTransmogrificationApplyRequestFrom(Player* player,
    std::map<uint8, uint32> const& data)
{
    if (!player || data.empty())
        return;

    uint32 result = 0;
    int32 price = 0;
    std::unordered_map<Item*, uint32> transmogrification;

    for (std::map<uint8, uint32>::const_iterator itr = data.begin(); itr != data.end(); ++itr)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, itr->first);
        ItemTemplate const* trans = sObjectMgr->GetItemTemplate(itr->second);
        if (!item || !trans || CanBeTransmogrifiedBy(player, item, trans) != TRANSMOGRIFICATION_ERROR_OK)
        {
            result = 1;
            transmogrification.clear();
            break;
        }

        price += GetPriceForItem(item->GetTemplate());
        transmogrification.emplace(item, itr->second);
    }

    if (result == 0)
    {
        if (player->HasEnoughMoney(price))
        {
            for (auto itr = transmogrification.begin(); itr != transmogrification.end(); ++itr)
            {
                if (Item* appearance = player->GetItemByEntry(itr->second))
                {
                    appearance->SetBinding(true);
                    player->RemoveTradeableItem(appearance);
                    appearance->ClearSoulboundTradeable(player);
                    appearance->SetNotRefundable(player);

                    itr->first->ClearSoulboundTradeable(player);
                    itr->first->SetNotRefundable(player);
                }

                UpdateItemTransmogrification(itr->first->GetGUID().GetCounter(), itr->second);
                UpdateItem(itr->first);
            }

            player->ModifyMoney(-price);
        }
        else
            result = 1;
    }

    player->SendAddonMessage(Acore::StringFormat("ASMSG_TRANSMOGRIFICATION_APPLY_RESPONSE\t{}", result));
}

std::string TransmogrificationMgr::GenerateTransmogrificationInfoFor(Player* player) const
{
    ASSERT(player);

    std::stringstream info;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        uint32 transEntry = GetItemTransmogrification(item->GetGUID().GetCounter());
        if (!transEntry)
            continue;

        info << uint32(i + 1) << ":" << transEntry << ";";
    }

    return info.str();
}

void TransmogrificationMgr::UpdateItem(Item* item)
{
    if (!item || !item->IsEquipped())
        return;

    Player* owner = item->GetOwner();
    if (!owner)
        return;

    owner->SetVisibleItemSlot(item->GetSlot(), item);
    if (owner->IsInWorld())
        item->SendUpdateToPlayer(owner);
}

uint32 TransmogrificationMgr::GetPriceForItem(ItemTemplate const* proto) const
{
    static std::map<uint32, uint32> const priceByItemLevel =
    {
        { 200, 50 * GOLD }, { 232, 100 * GOLD }, { 251, 150 * GOLD },
        { 277, 300 * GOLD }, { 290, 500 * GOLD }, { 303, 1000 * GOLD }
    };

    uint32 price = 50 * GOLD;
    for (std::map<uint32, uint32>::const_iterator itr = priceByItemLevel.begin(); itr != priceByItemLevel.end(); ++itr)
        if (proto->ItemLevel > itr->first)
            price = itr->second;

    return price;
}

namespace
{
    bool IsForbiddenTransmogInventory(uint32 inventoryType)
    {
        return inventoryType == INVTYPE_BAG || inventoryType == INVTYPE_RELIC || inventoryType == INVTYPE_BODY
            || inventoryType == INVTYPE_FINGER || inventoryType == INVTYPE_TRINKET || inventoryType == INVTYPE_AMMO
            || inventoryType == INVTYPE_QUIVER;
    }

    bool IsSameWeaponFamily(uint32 left, uint32 right, uint32 a, uint32 b, uint32 c, uint32 d)
    {
        auto inFamily = [&](uint32 subClass)
        {
            return subClass == a || subClass == b || subClass == c || subClass == d;
        };

        return inFamily(left) && inFamily(right);
    }
}

uint32 TransmogrificationMgr::CanBeTransmogrifiedBy(Player* player, Item* source, ItemTemplate const* trans) const
{
    if (!player || !source || !trans)
        return TRANSMOGRIFICATION_ERROR_DONT_REPORT;

    if (player->GetItemCount(trans->ItemId) == 0)
        return TRANSMOGRIFICATION_ERROR_DONT_REPORT;

    ItemTemplate const* sourceProto = source->GetTemplate();
    if (sourceProto->ItemId == trans->ItemId)
        return TRANSMOGRIFICATION_ERROR_CANNOT_ITEM_SELF;

    if (sourceProto->DisplayInfoID == trans->DisplayInfoID)
        return TRANSMOGRIFICATION_ERROR_SAME_APPEARANCE;

    if (sourceProto->Class != ITEM_CLASS_ARMOR && sourceProto->Class != ITEM_CLASS_WEAPON)
        return TRANSMOGRIFICATION_ERROR_CANNOT_BE_TRANSED;

    if (trans->Class != ITEM_CLASS_ARMOR && trans->Class != ITEM_CLASS_WEAPON)
        return TRANSMOGRIFICATION_ERROR_CANNOT_USE_FOR_TRANS;

    if (sourceProto->Class != trans->Class)
        return TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM;

    if (sourceProto->Quality != ITEM_QUALITY_UNCOMMON && sourceProto->Quality != ITEM_QUALITY_RARE
        && sourceProto->Quality != ITEM_QUALITY_EPIC)
        return TRANSMOGRIFICATION_ERROR_CANNOT_BE_TRANSED;

    if (trans->Quality != ITEM_QUALITY_POOR && trans->Quality != ITEM_QUALITY_NORMAL
        && trans->Quality != ITEM_QUALITY_UNCOMMON && trans->Quality != ITEM_QUALITY_RARE
        && trans->Quality != ITEM_QUALITY_EPIC)
        return TRANSMOGRIFICATION_ERROR_CANNOT_USE_FOR_TRANS;

    if (IsForbiddenTransmogInventory(sourceProto->InventoryType))
        return TRANSMOGRIFICATION_ERROR_CANNOT_BE_TRANSED;

    if (IsForbiddenTransmogInventory(trans->InventoryType))
        return TRANSMOGRIFICATION_ERROR_CANNOT_USE_FOR_TRANS;

    if (sourceProto->SubClass != trans->SubClass)
    {
        if (sourceProto->Class != ITEM_CLASS_WEAPON || trans->Class != ITEM_CLASS_WEAPON)
            return TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM;

        uint32 sourceSub = sourceProto->SubClass;
        uint32 transSub = trans->SubClass;
        bool oneHand = IsSameWeaponFamily(sourceSub, transSub,
            ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE,
            ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_FIST);
        bool twoHand = IsSameWeaponFamily(sourceSub, transSub,
            ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2,
            ITEM_SUBCLASS_WEAPON_POLEARM, ITEM_SUBCLASS_WEAPON_SWORD2);
        bool ranged = IsSameWeaponFamily(sourceSub, transSub,
            ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_GUN,
            ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_CROSSBOW);
        bool staff = IsSameWeaponFamily(sourceSub, transSub,
            ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_MACE2,
            ITEM_SUBCLASS_WEAPON_POLEARM, ITEM_SUBCLASS_WEAPON_POLEARM);
        bool dagger = IsSameWeaponFamily(sourceSub, transSub,
            ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD,
            ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD);
        if (!oneHand && !twoHand && !ranged && !staff && !dagger)
            return TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM;
    }

    if (sourceProto->InventoryType != trans->InventoryType)
    {
        if (sourceProto->Class == ITEM_CLASS_WEAPON)
        {
            bool sourceMelee = sourceProto->InventoryType == INVTYPE_WEAPON
                || sourceProto->InventoryType == INVTYPE_WEAPONMAINHAND
                || sourceProto->InventoryType == INVTYPE_WEAPONOFFHAND;
            bool transMelee = trans->InventoryType == INVTYPE_WEAPON
                || trans->InventoryType == INVTYPE_WEAPONMAINHAND
                || trans->InventoryType == INVTYPE_WEAPONOFFHAND;
            bool melee = sourceProto->SubClass != ITEM_SUBCLASS_WEAPON_FIST
                && trans->SubClass != ITEM_SUBCLASS_WEAPON_FIST && sourceMelee && transMelee;
            bool ranged = (trans->InventoryType == INVTYPE_RANGED || trans->InventoryType == INVTYPE_RANGEDRIGHT)
                && (sourceProto->InventoryType == INVTYPE_RANGED || sourceProto->InventoryType == INVTYPE_RANGEDRIGHT);
            if (!melee && !ranged)
                return TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM;
        }

        if (sourceProto->Class == ITEM_CLASS_ARMOR)
        {
            bool chest = (sourceProto->InventoryType == INVTYPE_CHEST || sourceProto->InventoryType == INVTYPE_ROBE)
                && (trans->InventoryType == INVTYPE_CHEST || trans->InventoryType == INVTYPE_ROBE);
            if (!chest)
                return TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM;
        }
    }

    return TRANSMOGRIFICATION_ERROR_OK;
}
