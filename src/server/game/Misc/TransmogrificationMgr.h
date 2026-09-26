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

#ifndef _TRANSMOGRIFICATION_H
#define _TRANSMOGRIFICATION_H

#include "Player.h"
#include <map>
#include <unordered_map>

enum TransmogrificationError
{
    TRANSMOGRIFICATION_ERROR_OK                        = 0,
    TRANSMOGRIFICATION_ERROR_DONT_REPORT               = 1,
    TRANSMOGRIFICATION_ERROR_CANNOT_ITEM_SELF          = 2,
    TRANSMOGRIFICATION_ERROR_SAME_APPEARANCE           = 3,
    TRANSMOGRIFICATION_ERROR_CANNOT_USE_FOR_TRANS      = 4,
    TRANSMOGRIFICATION_ERROR_CANNOT_USE_WITH_THIS_ITEM = 5,
    TRANSMOGRIFICATION_ERROR_CANNOT_BE_TRANSED         = 6,
    TRANSMOGRIFICATION_ERROR_NOT_ALLOWABLE             = 7
};

typedef std::unordered_map<uint64 /*itemGuid*/, uint32 /*transEntry*/> TransmogrificationContainer;

class TransmogrificationMgr
{
private:
    TransmogrificationMgr();
    ~TransmogrificationMgr();

public:
    static TransmogrificationMgr* instance();

    void LoadFromDB();

    uint32 GetItemTransmogrification(uint64 guid) const;
    void UpdateItemTransmogrification(uint64 guid, uint32 transEntry);
    void RemoveItemTransmogrification(uint64 guid);

    void RemoveAllTransmogrificationByEntry(Player* player, uint32 entry);

    void SendTransmogrificationMenuOpenTo(Player* player, Creature* creature);
    void SendTransmogrificationMenuCloseTo(Player* player);

    void HandleTransmogrificationPrepareRequestFrom(Player* player, uint8 pos, uint32 transEntry);
    void HandleTransmogrificationRemoveRequestFrom(Player* player, uint8 pos);
    void HandleTransmogrificationApplyRequestFrom(Player* player, std::map<uint8, uint32> const& data);

    std::string GenerateTransmogrificationInfoFor(Player* player) const;

private:
    void UpdateItem(Item* item);
    uint32 GetPriceForItem(ItemTemplate const* proto) const;
    uint32 CanBeTransmogrifiedBy(Player* player, Item* source, ItemTemplate const* trans) const;

    TransmogrificationContainer _transmogrificationStore;
};

#define sTransmogrificationMgr TransmogrificationMgr::instance()

#endif
