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

#ifndef _ADDONIO_H
#define _ADDONIO_H

#include "Player.h"

class AddonIO
{
private:
    AddonIO();
    ~AddonIO();

public:
    static AddonIO* instance();

    void HandleMessage(Player* player, std::string const& message);

    void HandleTransmogrificationInfoRequest(Player* player, std::string const& body);
    void HandleTransmogrificationPrepareRequest(Player* player, std::string const& body);
    void HandleTransmogrificationRemove(Player* player, std::string const& body);
    void HandleTransmogrificationApply(Player* player, std::string const& body);

    void HandleShopBalanceRequest(Player* player, std::string const& body);
    void HandlePremiumInfoRequest(Player* player, std::string const& body);
    void HandlePremiumRenewRequest(Player* player, std::string const& body);
    void HandleShopItemListRequest(Player* player, std::string const& body);
    void HandleShopVersionRequest(Player* player, std::string const& body);
    void HandleShopBuyItemRequest(Player* player, std::string const& body);
    void HandleShopSpecialOfferListRequest(Player* player, std::string const& body);
    void HandleShopCollectionLoadRequest(Player* player, std::string const& body);
    void HandleShopItemCountRequest(Player* player, std::string const& body);

    void HandleAverageItemLevelRequest(Player* player, std::string const& body);
};

typedef void (AddonIO::*AddonMessageHandler)(Player*, std::string const&);

#define sAddonIO AddonIO::instance()

#endif
