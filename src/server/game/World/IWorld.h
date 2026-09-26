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

#ifndef AZEROTHCORE_IWORLD_H
#define AZEROTHCORE_IWORLD_H

#include "AsyncCallbackProcessor.h"
#include "Common.h"
#include "Duration.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "WorldConfig.h"
#include <map>
#include <unordered_map>

class WorldPacket;
class WorldSession;
class Player;

/// Storage class for commands issued for delayed execution
struct AC_GAME_API CliCommandHolder
{
    using Print = void(*)(void*, std::string_view);
    using CommandFinished = void(*)(void*, bool success);

    void* m_callbackArg;
    char* m_command;
    Print m_print;
    CommandFinished m_commandFinished;

    CliCommandHolder(void* callbackArg, char const* command, Print zprint, CommandFinished commandFinished);
    ~CliCommandHolder();

private:
    CliCommandHolder(CliCommandHolder const& right) = delete;
    CliCommandHolder& operator=(CliCommandHolder const& right) = delete;
};

// ServerMessages.dbc
enum ServerMessageType
{
    SERVER_MSG_SHUTDOWN_TIME      = 1,
    SERVER_MSG_RESTART_TIME       = 2,
    SERVER_MSG_STRING             = 3,
    SERVER_MSG_SHUTDOWN_CANCELLED = 4,
    SERVER_MSG_RESTART_CANCELLED  = 5
};

struct PlayerDonate
{
    uint32 balance = 0;
    uint32 vote = 0;
};

struct StoreItemData
{
    uint32 itemEntry = 0;
    uint32 count = 0;
    uint32 price = 0;
    uint8 discount = 0;
    uint32 discountPrice = 0;
    uint32 creatureEntry = 0;
    uint32 storeFlags = 0;
    uint8 CategoryID = 0;
    uint8 SubCategoryID = 0;
    uint8 MoneyID = 0;
};

struct StoreSpecialOfferData
{
    std::string background;
    std::string headline;
    std::string title;
    std::string description;
    std::string detailsTitle;
    uint32 details = 0;
    uint32 time = 0;
    uint32 productID = 0;
    uint32 itemEntry = 0;
    uint32 price = 0;
};

struct StoreSpecialOfferDetailsData
{
    uint32 itemID = 0;
    uint32 role = 0;
    uint32 count = 0;
};

struct CollectionMountData
{
    uint32 id = 0;
    std::string hash;
    uint8 currency = 0;
    uint32 price = 0;
    uint32 productID = 0;
};

typedef std::map<uint32, PlayerDonate> PlayerDonateMap;

class IWorld
{
public:
    virtual ~IWorld() = default;
    [[nodiscard]] virtual bool IsClosed() const = 0;
    virtual void SetClosed(bool val) = 0;
    [[nodiscard]] virtual AccountTypes GetPlayerSecurityLimit() const = 0;
    virtual void SetPlayerSecurityLimit(AccountTypes sec) = 0;
    virtual void LoadDBAllowedSecurityLevel() = 0;
    [[nodiscard]] virtual bool getAllowMovement() const = 0;
    virtual void SetAllowMovement(bool allow) = 0;
    [[nodiscard]] virtual LocaleConstant GetDefaultDbcLocale() const = 0;
    [[nodiscard]] virtual std::string const& GetDataPath() const = 0;
    [[nodiscard]] virtual Seconds GetNextDailyQuestsResetTime() const = 0;
    [[nodiscard]] virtual Seconds GetNextWeeklyQuestsResetTime() const = 0;
    [[nodiscard]] virtual Seconds GetNextRandomBGResetTime() const = 0;
    [[nodiscard]] virtual uint16 GetConfigMaxSkillValue() const = 0;
    virtual void SetInitialWorldSettings() = 0;
    virtual void LoadConfigSettings(bool reload = false) = 0;
    [[nodiscard]] virtual bool IsShuttingDown() const = 0;
    [[nodiscard]] virtual uint32 GetShutDownTimeLeft() const = 0;
    virtual void ShutdownServ(uint32 time, uint32 options, uint8 exitcode, std::string const& reason = std::string()) = 0;
    virtual void ShutdownCancel() = 0;
    virtual void ShutdownMsg(bool show = false, Player* player = nullptr, std::string const& reason = std::string()) = 0;
    virtual void Update(uint32 diff) = 0;
    virtual void setRate(ServerConfigs index, float value) = 0;
    [[nodiscard]] virtual float getRate(ServerConfigs index) const = 0;
    virtual void setBoolConfig(ServerConfigs index, bool value) = 0;
    [[nodiscard]] virtual bool getBoolConfig(ServerConfigs index) const = 0;
    virtual void setFloatConfig(ServerConfigs index, float value) = 0;
    [[nodiscard]] virtual float getFloatConfig(ServerConfigs index) const = 0;
    virtual void setIntConfig(ServerConfigs index, uint32 value) = 0;
    [[nodiscard]] virtual uint32 getIntConfig(ServerConfigs index) const = 0;
    virtual void setStringConfig(ServerConfigs index, std::string const& value) = 0;
    virtual std::string_view getStringConfig(ServerConfigs index) const = 0;
    [[nodiscard]] virtual bool IsPvPRealm() const = 0;
    [[nodiscard]] virtual bool IsFFAPvPRealm() const = 0;
    virtual uint32 GetNextWhoListUpdateDelaySecs() = 0;
    virtual void ProcessCliCommands() = 0;
    virtual void QueueCliCommand(CliCommandHolder* commandHolder) = 0;
    virtual void ForceGameEventUpdate() = 0;
    virtual void UpdateRealmCharCount(uint32 accid) = 0;
    [[nodiscard]] virtual LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const = 0;
    virtual void LoadDBVersion() = 0;
    [[nodiscard]] virtual char const* GetDBVersion() const = 0;
    virtual void UpdateAreaDependentAuras() = 0;
    [[nodiscard]] virtual uint32 GetCleaningFlags() const = 0;
    virtual void   SetCleaningFlags(uint32 flags) = 0;
    virtual void   ResetEventSeasonalQuests(uint16 event_id) = 0;
    [[nodiscard]] virtual std::string const& GetRealmName() const = 0;
    virtual void SetRealmName(std::string name) = 0;
    virtual void ReloadRBAC() = 0;

    virtual void LoadShop() = 0;
    virtual void LoadDonateCurrency() = 0;
    virtual PlayerDonate FindShopCurrency(uint32 accountId) const = 0;
    [[nodiscard]] virtual uint32 GetStoreItems() const = 0;
    [[nodiscard]] virtual std::map<int32, StoreItemData> const& GetStoreItem() const = 0;
    [[nodiscard]] virtual std::map<int32, StoreSpecialOfferData> const& GetStoreSpecialOffer() const = 0;
    [[nodiscard]] virtual std::multimap<int32, StoreSpecialOfferDetailsData> const&
        GetStoreSpecialDetails() const = 0;
    [[nodiscard]] virtual std::map<int32, CollectionMountData> const& GetStoreCollection() const = 0;
    [[nodiscard]] virtual uint32 GetShopVersion() const = 0;
};

#endif //AZEROTHCORE_IWORLD_H
