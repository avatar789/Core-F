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

#include "AddonIO.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TransmogrificationMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <boost/algorithm/string.hpp>
#include <initializer_list>

namespace
{
    constexpr uint8 ARMORY_CATEGORY_ID = 4;

    enum StoreService
    {
        PAID_SERVICE_NAME_CHANGE         = 1000000,
        PAID_SERVICE_FACTION_CHANGE      = 1000001,
        PAID_SERVICE_RACE_CHANGE         = 1000002,
        PAID_SERVICE_GUILDNAME_CHANGE    = 1000003,
        PAID_SERVICE_GOLD_BUY            = 1000004,
        PAID_SERVICE_ALCHEMY_LEARN       = 1000005,
        PAID_SERVICE_BLACKSMITHING_LEARN = 1000006,
        PAID_SERVICE_ENCHANTING_LEARN    = 1000007,
        PAID_SERVICE_ENGINEERING_LEARN   = 1000008,
        PAID_SERVICE_JEWELCRAFTING_LEARN = 1000009,
        PAID_SERVICE_HERBALISM_LEARN     = 1000010,
        PAID_SERVICE_LEATHERWORKING_LEARN = 1000011,
        PAID_SERVICE_MINING_LEARN        = 1000012,
        PAID_SERVICE_SKINNING_LEARN      = 1000013,
        PAID_SERVICE_TAILORING_LEARN     = 1000014,
        PAID_SERVICE_FISHING_LEARN       = 1000015,
        PAID_SERVICE_INSCRIPTION_LEARN   = 1000016,
        PAID_SERVICE_COOKING_LEARN       = 1000017,
        PAID_SERVICE_FIRST_AID_LEARN     = 1000018,
        PAID_SERVICE_PREMIUM_ONE_DAY     = 1000019,
        PAID_SERVICE_LEVELUP             = 1000020
    };

    std::unordered_map<std::string, AddonMessageHandler> addonMessagesTable =
    {
        { "ACMSG_TRANSMOGRIFICATION_INFO_REQUEST",    &AddonIO::HandleTransmogrificationInfoRequest },
        { "ACMSG_TRANSMOGRIFICATION_PREPARE_REQUEST", &AddonIO::HandleTransmogrificationPrepareRequest },
        { "ACMSG_TRANSMOGRIFICATION_REMOVE",          &AddonIO::HandleTransmogrificationRemove },
        { "ACMSG_TRANSMOGRIFICATION_APPLY",           &AddonIO::HandleTransmogrificationApply },
        { "ACMSG_AVERAGE_ITEM_LEVEL_REQUEST",         &AddonIO::HandleAverageItemLevelRequest },
        { "ACMSG_SHOP_BALANCE_REQUEST",               &AddonIO::HandleShopBalanceRequest },
        { "ACMSG_SHOP_ITEM_LIST_REQUEST",             &AddonIO::HandleShopItemListRequest },
        { "ACMSG_SHOP_VERSION",                       &AddonIO::HandleShopVersionRequest },
        { "ACMSG_SHOP_BUY_ITEM",                      &AddonIO::HandleShopBuyItemRequest },
        { "ACMSG_SHOP_SPECIAL_OFFER_LIST_REQUEST",    &AddonIO::HandleShopSpecialOfferListRequest },
        { "ACMSG_SHOP_COLLECTION_LOAD_REQUEST",       &AddonIO::HandleShopCollectionLoadRequest },
        { "ACMSG_SHOP_ITEM_COUNT",                    &AddonIO::HandleShopItemCountRequest }
    };

    std::vector<std::string> SplitAddonArgs(std::string const& value, char const* delimiter)
    {
        std::vector<std::string> args;
        boost::split(args, value, boost::is_any_of(delimiter));
        return args;
    }

    uint8 ShopProfessionResponse(Player* player, SkillType skill)
    {
        if (player->GetLevel() < DEFAULT_MAX_LEVEL)
            return 7;

        if (player->PlayerAlreadyHasTwoProfessions(player) && !player->IsSecondarySkill(skill))
            return 6;

        if (player->LearnAllRecipesInProfession(player, skill))
            return 0;

        return 1;
    }

    bool ShopSendItem(Player* senderPlayer, Player* receiver, std::string const& text, uint32 itemId, uint32 count)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto || !receiver)
            return false;

        if (count < 1 || (proto->MaxCount > 0 && count > uint32(proto->MaxCount)))
            return false;

        using ItemPair = std::pair<uint32, uint32>;
        std::list<ItemPair> items;
        uint32 remaining = count;
        while (remaining > proto->GetMaxStackSize())
        {
            items.emplace_back(itemId, proto->GetMaxStackSize());
            remaining -= proto->GetMaxStackSize();
        }

        items.emplace_back(itemId, remaining);
        if (items.size() > MAX_MAIL_ITEMS)
            return false;

        MailSender sender(MAIL_NORMAL, senderPlayer->GetGUID().GetCounter(), MAIL_STATIONERY_GM);
        MailDraft draft("Refund", text);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (ItemPair const& pair : items)
        {
            if (Item* item = Item::CreateItem(pair.first, pair.second, senderPlayer))
            {
                item->SaveToDB(trans);
                draft.AddItem(item);
            }
        }

        draft.SendMailTo(trans, MailReceiver(receiver), sender);
        CharacterDatabase.CommitTransaction(trans);
        return true;
    }

    uint32 ShopNow()
    {
        return uint32(GameTime::GetGameTime().count());
    }

    uint8 ShopAddItem(Player* player, Player* receiver, uint32 itemId, uint32 count, uint8 moneyId, uint32 cost,
        std::string const& text = "")
    {
        if (!sObjectMgr->GetItemTemplate(itemId))
            return 4;

        if (moneyId == 10)
        {
            if (!player->ModifyMoney(-int32(cost)))
                return 1;
        }
        else if (!player->GetSession()->SetAccountCurrency(cost, moneyId, false))
            return 1;
        else if (moneyId == 1)
            player->GetSession()->WritePurchaseToLogs(player->GetSession(), "BUY ITEM", itemId, count, cost, ShopNow());

        uint32 noSpaceForCount = 0;
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
            count -= noSpaceForCount;

        Item* item = player->StoreNewItem(dest, itemId, true);
        for (ItemPosCount const& pos : dest)
            if (Item* stored = player->GetItemByPos(pos.pos))
                stored->SetBinding(false);

        if (count > 0 && item)
        {
            if (text.empty())
                player->SendNewItem(item, count, false, true);
            else
                ShopSendItem(player, receiver, text, itemId, count);
        }

        if (noSpaceForCount > 0)
            ShopSendItem(player, player, "Return of lost items", itemId, noSpaceForCount);

        return 0;
    }

    void RemoveProfessionSpecializations(Player* player, std::initializer_list<uint32> spells)
    {
        for (uint32 spellId : spells)
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    uint8 ShopPaidService(Player* player, uint32 itemId, uint32 count, uint8 moneyId, uint32 cost, bool isProfession)
    {
        WorldSession* session = player->GetSession();
        uint8 response = 0;

        if (!session->SetAccountCurrency(cost, moneyId, isProfession))
            return 1;

        uint32 atLoginFlag = 0;
        bool isRaceFaction = false;

        switch (itemId)
        {
            case PAID_SERVICE_NAME_CHANGE:
                atLoginFlag = AT_LOGIN_RENAME;
                player->SetAtLoginFlag(AT_LOGIN_RENAME);
                isRaceFaction = true;
                session->WritePurchaseToLogs(session, "NAME_CHANGE", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_FACTION_CHANGE:
                atLoginFlag = AT_LOGIN_CHANGE_FACTION;
                player->SetAtLoginFlag(AT_LOGIN_CHANGE_FACTION);
                isRaceFaction = true;
                session->WritePurchaseToLogs(session, "FACTION_CHANGE", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_RACE_CHANGE:
                atLoginFlag = AT_LOGIN_CHANGE_RACE;
                player->SetAtLoginFlag(AT_LOGIN_CHANGE_RACE);
                isRaceFaction = true;
                session->WritePurchaseToLogs(session, "RACE_CHANGE", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_GOLD_BUY:
                player->ModifyMoney(int32(count) * int32(GOLD) * 1000);
                session->WritePurchaseToLogs(session, "GOLD_BUY", 0, count, cost, ShopNow());
                break;
            case PAID_SERVICE_ALCHEMY_LEARN:
                response = ShopProfessionResponse(player, SKILL_ALCHEMY);
                if (response == 0)
                {
                    session->WritePurchaseToLogs(session, "ALCHEMY_LEARN", 0, 0, cost, ShopNow());
                    RemoveProfessionSpecializations(player, { 28675, 28677, 28672 });
                }
                break;
            case PAID_SERVICE_BLACKSMITHING_LEARN:
                response = ShopProfessionResponse(player, SKILL_BLACKSMITHING);
                if (response == 0)
                {
                    session->WritePurchaseToLogs(session, "BLACKSMITHING_LEARN", 0, 0, cost, ShopNow());
                    RemoveProfessionSpecializations(player, { 9787, 17041, 17040, 17039, 9788 });
                }
                break;
            case PAID_SERVICE_ENCHANTING_LEARN:
                response = ShopProfessionResponse(player, SKILL_ENCHANTING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "ENCHANTING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_ENGINEERING_LEARN:
                response = ShopProfessionResponse(player, SKILL_ENGINEERING);
                if (response == 0)
                {
                    session->WritePurchaseToLogs(session, "ENGINEERING_LEARN", 0, 0, cost, ShopNow());
                    RemoveProfessionSpecializations(player, { 20222, 20219 });
                }
                break;
            case PAID_SERVICE_JEWELCRAFTING_LEARN:
                response = ShopProfessionResponse(player, SKILL_JEWELCRAFTING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "JEWELCRAFTING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_HERBALISM_LEARN:
                response = ShopProfessionResponse(player, SKILL_HERBALISM);
                if (response == 0)
                {
                    session->WritePurchaseToLogs(session, "HERBALISM_LEARN", 0, 0, cost, ShopNow());
                    RemoveProfessionSpecializations(player, { 2369, 2371 });
                }
                break;
            case PAID_SERVICE_LEATHERWORKING_LEARN:
                response = ShopProfessionResponse(player, SKILL_LEATHERWORKING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "LEATHERWORKING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_MINING_LEARN:
                response = ShopProfessionResponse(player, SKILL_MINING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "MINING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_SKINNING_LEARN:
                response = ShopProfessionResponse(player, SKILL_SKINNING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "SKINNING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_TAILORING_LEARN:
                response = ShopProfessionResponse(player, SKILL_TAILORING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "TAILORING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_FISHING_LEARN:
                response = ShopProfessionResponse(player, SKILL_FISHING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "FISHING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_INSCRIPTION_LEARN:
                response = ShopProfessionResponse(player, SKILL_INSCRIPTION);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "INSCRIPTION_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_FIRST_AID_LEARN:
                response = ShopProfessionResponse(player, SKILL_FIRST_AID);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "FIRST_AID_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_COOKING_LEARN:
                response = ShopProfessionResponse(player, SKILL_COOKING);
                if (response == 0)
                    session->WritePurchaseToLogs(session, "COOKING_LEARN", 0, 0, cost, ShopNow());
                break;
            case PAID_SERVICE_LEVELUP:
                player->GiveLevel(uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)));
                break;
            default:
                break;
        }

        if (isRaceFaction)
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(
                CHAR_UPD_ADD_AT_LOGIN_FLAG);
            stmt->SetData(0, uint16(atLoginFlag));
            stmt->SetData(1, player->GetGUID().GetCounter());
            CharacterDatabase.Execute(stmt);
        }

        if (isProfession && response == 0)
            session->SetAccountCurrency(cost, moneyId, false);

        return response;
    }

    bool IsPaidService(uint32 itemId)
    {
        switch (itemId)
        {
            case PAID_SERVICE_NAME_CHANGE:
            case PAID_SERVICE_FACTION_CHANGE:
            case PAID_SERVICE_RACE_CHANGE:
            case PAID_SERVICE_GUILDNAME_CHANGE:
            case PAID_SERVICE_GOLD_BUY:
            case PAID_SERVICE_PREMIUM_ONE_DAY:
            case PAID_SERVICE_LEVELUP:
            case PAID_SERVICE_ALCHEMY_LEARN:
            case PAID_SERVICE_BLACKSMITHING_LEARN:
            case PAID_SERVICE_ENCHANTING_LEARN:
            case PAID_SERVICE_ENGINEERING_LEARN:
            case PAID_SERVICE_JEWELCRAFTING_LEARN:
            case PAID_SERVICE_HERBALISM_LEARN:
            case PAID_SERVICE_LEATHERWORKING_LEARN:
            case PAID_SERVICE_MINING_LEARN:
            case PAID_SERVICE_SKINNING_LEARN:
            case PAID_SERVICE_TAILORING_LEARN:
            case PAID_SERVICE_FISHING_LEARN:
            case PAID_SERVICE_INSCRIPTION_LEARN:
            case PAID_SERVICE_FIRST_AID_LEARN:
            case PAID_SERVICE_COOKING_LEARN:
                return true;
            default:
                return false;
        }
    }

    bool IsProfessionService(uint32 itemId)
    {
        switch (itemId)
        {
            case PAID_SERVICE_ALCHEMY_LEARN:
            case PAID_SERVICE_BLACKSMITHING_LEARN:
            case PAID_SERVICE_ENCHANTING_LEARN:
            case PAID_SERVICE_ENGINEERING_LEARN:
            case PAID_SERVICE_JEWELCRAFTING_LEARN:
            case PAID_SERVICE_HERBALISM_LEARN:
            case PAID_SERVICE_LEATHERWORKING_LEARN:
            case PAID_SERVICE_MINING_LEARN:
            case PAID_SERVICE_SKINNING_LEARN:
            case PAID_SERVICE_TAILORING_LEARN:
            case PAID_SERVICE_FISHING_LEARN:
            case PAID_SERVICE_INSCRIPTION_LEARN:
            case PAID_SERVICE_FIRST_AID_LEARN:
            case PAID_SERVICE_COOKING_LEARN:
                return true;
            default:
                return false;
        }
    }
}

AddonIO::AddonIO() { }

AddonIO::~AddonIO() { }

AddonIO* AddonIO::instance()
{
    static AddonIO instance;
    return &instance;
}

void AddonIO::HandleMessage(Player* player, std::string const& message)
{
    if (!player)
        return;

    std::vector<std::string> args = SplitAddonArgs(message, "\t");
    if (args.size() != 2)
        return;

    auto itr = addonMessagesTable.find(args[0]);
    if (itr == addonMessagesTable.end())
        return;

    (this->*itr->second)(player, args[1]);
}

void AddonIO::HandleTransmogrificationInfoRequest(Player* player, std::string const& body)
{
    if (!player || body.empty())
        return;

    ObjectGuid guid = ObjectGuid::Empty;
    try
    {
        guid = ObjectGuid(std::stoull(body, nullptr, 16));
    }
    catch (std::exception const&)
    {
        return;
    }

    Player* target = ObjectAccessor::GetPlayer(*player, guid);
    if (!target)
        return;

    player->SendAddonMessage(Acore::StringFormat("ASMSG_TRANSMOGRIFICATION_INFO_RESPONSE\t{};{}",
        guid.GetRawValue(), sTransmogrificationMgr->GenerateTransmogrificationInfoFor(target)));
}

void AddonIO::HandleTransmogrificationPrepareRequest(Player* player, std::string const& body)
{
    if (!player || body.empty())
        return;

    std::vector<std::string> args = SplitAddonArgs(body, ":");
    if (args.size() != 2)
        return;

    try
    {
        int slot = std::stoi(args[0]) - 1;
        uint32 transEntry = uint32(std::stoul(args[1]));
        if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
            return;

        sTransmogrificationMgr->HandleTransmogrificationPrepareRequestFrom(player, uint8(slot), transEntry);
    }
    catch (std::exception const&)
    {
        return;
    }
}

void AddonIO::HandleTransmogrificationRemove(Player* player, std::string const& body)
{
    if (!player || body.empty())
        return;

    try
    {
        int slot = std::stoi(body) - 1;
        if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
            return;

        sTransmogrificationMgr->HandleTransmogrificationRemoveRequestFrom(player, uint8(slot));
    }
    catch (std::exception const&)
    {
        return;
    }
}

void AddonIO::HandleTransmogrificationApply(Player* player, std::string const& body)
{
    if (!player || body.empty())
        return;

    std::map<uint8, uint32> data;
    try
    {
        for (std::string const& pairText : SplitAddonArgs(body, ";"))
        {
            std::vector<std::string> pair = SplitAddonArgs(pairText, ":");
            if (pair.size() != 2)
                continue;

            int slot = std::stoi(pair[0]) - 1;
            if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
                return;

            data.emplace(uint8(slot), uint32(std::stoul(pair[1])));
        }
    }
    catch (std::exception const&)
    {
        return;
    }

    sTransmogrificationMgr->HandleTransmogrificationApplyRequestFrom(player, data);
}

void AddonIO::HandleAverageItemLevelRequest(Player* player, std::string const& body)
{
    if (!player)
        return;

    if (body.empty())
        return;

    ObjectGuid guid = ObjectGuid::Empty;
    try
    {
        guid = ObjectGuid(std::stoull(body, nullptr, 16));
    }
    catch (std::exception const&)
    {
        player->SendAddonMessage("ASMSG_AVERAGE_ITEM_LEVEL_RESPONSE\t-1");
        return;
    }

    Player* target = ObjectAccessor::FindPlayer(guid);
    if (!target || player->IsValidAttackTarget(target))
    {
        player->SendAddonMessage("ASMSG_AVERAGE_ITEM_LEVEL_RESPONSE\t-1");
        return;
    }

    target->CalculateAverageItemLevel();
    player->SendAddonMessage(Acore::StringFormat("ASMSG_AVERAGE_ITEM_LEVEL_RESPONSE\t{}",
        uint32(target->GetAverageItemLevel())));
}

void AddonIO::HandleShopBalanceRequest(Player* player, std::string const& /*body*/)
{
    if (!player)
        return;

    WorldSession* session = player->GetSession();
    player->SendAddonMessage(Acore::StringFormat("ASMSG_SHOP_BALANCE_RESPONSE\t{}:{}:0:0:0:0:0",
        session->GetAccountBalance(), session->GetAccountVote()));
}

void AddonIO::HandleShopItemListRequest(Player* player, std::string const& /*body*/)
{
    if (!player || !sWorld->getBoolConfig(CONFIG_SHOP_ENABLE))
        return;

    for (auto const& it : sWorld->GetStoreItem())
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(it.second.itemEntry);
        if (!proto)
            continue;

        if (it.second.CategoryID == ARMORY_CATEGORY_ID)
        {
            if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_PLATE
                && !player->HasSpell(750))
                continue;

            if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_MAIL
                && !player->HasSpell(8737))
                continue;

            if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_LEATHER
                && !player->HasSpell(9077))
                continue;

            if (!(1 << (player->getClass() - 1) & proto->AllowableClass))
                continue;

            if (!(1 << (player->getRace() - 1) & proto->AllowableRace))
                continue;
        }

        player->SendAddonMessage(Acore::StringFormat(
            "ASMSG_SHOP_ITEM\t{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}",
            it.first, it.second.itemEntry, it.second.count, it.second.price, it.second.discount,
            it.second.discountPrice, it.second.creatureEntry, it.second.storeFlags,
            it.second.CategoryID, it.second.SubCategoryID, it.second.MoneyID));
    }
}

void AddonIO::HandleShopVersionRequest(Player* player, std::string const& /*body*/)
{
    if (!player)
        return;

    player->SendAddonMessage(Acore::StringFormat("ASMSG_SHOP_VERSION\t{}:{}",
        sWorld->GetShopVersion(), sWorld->getBoolConfig(CONFIG_SHOP_ENABLE) ? 1 : 0));
}

void AddonIO::HandleShopBuyItemRequest(Player* player, std::string const& body)
{
    if (!player || body.empty() || !sWorld->getBoolConfig(CONFIG_SHOP_ENABLE))
        return;

    WorldSession* session = player->GetSession();
    uint8 response = 1;
    uint32 item = 0;

    try
    {
        std::vector<std::string> par = SplitAddonArgs(body, ":");
        if (par.empty())
            return;

        uint32 cost = 0;
        uint32 count = 1;
        uint32 dbCount = 1;
        uint32 moneyId = 1;

        for (auto const& it : sWorld->GetStoreItem())
        {
            if (it.first != std::stoi(par[0]))
                continue;

            item = it.second.itemEntry;
            cost = it.second.discountPrice;
            dbCount = it.second.count;
            moneyId = it.second.MoneyID;
            if (par.size() > 1)
                count = uint32(std::stoul(par[1]));
            break;
        }

        int32 balance = moneyId == 1 ? session->GetAccountBalance() : session->GetAccountVote();
        if (dbCount > 1 && dbCount != count)
            count = dbCount;

        uint32 finalCost = dbCount == 1 ? cost * count : cost;
        if (balance > 0 && balance >= int32(finalCost))
        {
            switch (par.size())
            {
                case 4:
                    if (IsProfessionService(item))
                        response = ShopPaidService(player, item, count, uint8(moneyId), finalCost, true);
                    else if (IsPaidService(item))
                        response = ShopPaidService(player, item, count, uint8(moneyId), finalCost, false);
                    else
                        response = ShopAddItem(player, player, item, count, uint8(moneyId), finalCost);
                    break;
                case 6:
                    if (!IsPaidService(item))
                    {
                        Player* target = nullptr;
                        std::string targetName;
                        uint32 targetGuid = uint32(std::strtoul(par[3].c_str(), nullptr, 10));
                        ObjectGuid parseGUID = ObjectGuid::Create<HighGuid::Player>(targetGuid);
                        if (sCharacterCache->GetCharacterNameByGuid(parseGUID, targetName))
                            target = ObjectAccessor::FindPlayer(parseGUID);

                        if (!target)
                            response = 2;
                        else if (player == target)
                            response = 3;
                        else
                            response = ShopAddItem(player, target, item, count, uint8(moneyId), finalCost, par[5]);
                    }
                    break;
                default:
                    break;
            }
        }

        player->SendAddonMessage(Acore::StringFormat("ASMSG_SHOP_BUY_ITEM_RESPONSE\t{}:{}", response, item));
    }
    catch (std::exception const&)
    {
        return;
    }
}

void AddonIO::HandleShopSpecialOfferListRequest(Player* player, std::string const& /*body*/)
{
    if (!player)
        return;

    std::string response;
    for (auto const& it : sWorld->GetStoreSpecialDetails())
    {
        if (it.first != 0)
            continue;

        response += Acore::StringFormat("{}<{}><{}>:", it.second.itemID, it.second.role, it.second.count);
    }

    if (!response.empty())
        player->SendAddonMessage(response);
}

void AddonIO::HandleShopCollectionLoadRequest(Player* player, std::string const& /*body*/)
{
    if (!player)
        return;

    for (auto const& it : sWorld->GetStoreItem())
        if (it.second.creatureEntry != 0)
            player->GetSession()->ShopCreatureOpcode(it.second.creatureEntry);
}

void AddonIO::HandleShopItemCountRequest(Player* player, std::string const& /*body*/)
{
    if (!player || !sWorld->getBoolConfig(CONFIG_SHOP_ENABLE))
        return;

    player->SendAddonMessage(Acore::StringFormat("ASMSG_SHOP_ITEM_COUNT\t{}", sWorld->GetStoreItems()));
}
