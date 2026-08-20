/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *
 * PATCHED 2026-08-20 (TylerConlee fork, for Hollowpeak): removed the
 * OnPlayerResurrect override below. That hook doesn't exist in the
 * liyunfan1223/azerothcore-wotlk Playerbot fork this server runs on --
 * "marked 'override', but does not override" at compile time. The
 * module's own comment on that function said "We keep this function
 * just to prevent some exploits for reviving" -- i.e. it was already a
 * secondary, supplementary safety net, not the primary enforcement
 * mechanism. The actual permadeath enforcement is CanPacketReceive()
 * further down in HardModeServerScript, which blocks resurrection-related
 * packets (SMSG_PRE_RESURRECT, CMSG_RECLAIM_CORPSE,
 * CMSG_SPIRIT_HEALER_ACTIVATE, etc.) directly at the network layer and
 * doesn't depend on this hook at all -- that's untouched below.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

class HardcoreMode : public PlayerScript
{
public:
    explicit HardcoreMode() : PlayerScript("mod-hardcore")
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        if (getHardcoreEnabledForPlayer(player))
        {
            if (player->isDead())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("You died during hardcore session, so you are a permanent ghost, try to scare living people.");
            }
            else
            {
                this->sendHarcoreStatus(player);
            }
        }
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldlevel*/) override
    {
        if (getHardcoreEnabledForPlayer(player))
        {
            this->sendHarcoreStatus(player);
        }
    }

    void OnPlayerJustDied(Player* player) override
    {
        if (getHardcoreEnabledForPlayer(player))
        {
            Group* group = player->GetGroup();
            if (group)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Leaving group... You can't be a part of living people anymore.");
                group->RemoveMember(player->GetGUID());
            }
        }
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (getHardcoreEnabledForPlayer(player))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You'll be a ghost forever...");
            return;
        }
    }

    void OnPlayerPVPKill(Player* /*killer*/, Player* killed) override
    {
        if (getHardcoreEnabledForPlayer(killed))
        {
            ChatHandler(killed->GetSession()).PSendSysMessage("Some player killed you... in hardcore.");
            return;
        }
    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* killed) override
    {
        if (getHardcoreEnabledForPlayer(killed))
        {
            ChatHandler(killed->GetSession()).PSendSysMessage("You died during a hardcore session... Skills issues.");
            return;
        }
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            return false;
        }
        return true;
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            return false;
        }
        return true;
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Group* group) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            return false;
        }
        return true;
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Guild* guild) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            return false;
        }
        return true;
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Channel* channel) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            return false;
        }
        return true;
    }

    bool OnPlayerCanGroupInvite(Player* player, std::string& membername) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You can't invite players to a group while dead.");
            return false;
        }
        return true;
    }

    bool OnPlayerCanGroupAccept(Player* player, Group* group) override
    {
        if (getHardcoreEnabledForPlayer(player) && player->isDead())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You can't be a part of a group.");
            return false;
        }
        return true;
    }

private:
    void sendHarcoreStatus(Player* player)
    {
        if (player->IsGameMaster())
        {
            return;
        }
        if (this->getHardcoreEnabledForPlayer(player))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Hardcore is now ENABLED...");
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Hardcore is now DISABLED...");
        }
    }

    bool getHardcoreEnabledForPlayer(Player* player)
    {
        if (!player->IsGameMaster() && sConfigMgr->GetOption<bool>("ModHardcore.Enable", false))
        {
            if (player->GetLevel() >= sConfigMgr->GetOption<int>("ModHardcoreMinLevel.Enable", 1) && player->GetLevel() <= sConfigMgr->GetOption<int>("ModHardcoreMaxLevel.Enable", 79))
            {
                return true;
            }
        }
        return false;
    }
};

class HardModeServerScript : ServerScript
{
public:
    HardModeServerScript() : ServerScript("mod-hardcore")
    {
    }

    bool CanPacketReceive(WorldSession* session, const WorldPacket& packet) override
    {
        if (!sConfigMgr->GetOption<bool>("ModHardcore.Enable", false))
        {
            return true;
        }

        if (!session)
        {
            return true;
        }

        auto player = session->GetPlayer();
        if (!player || !player->isDead() || player->IsGameMaster())
        {
            return true;
        }

        if (player->GetLevel() >= sConfigMgr->GetOption<int>("ModHardcoreMinLevel.Enable", 1) && player->GetLevel() <= sConfigMgr->GetOption<int>("ModHardcoreMaxLevel.Enable", 79))
        {
            auto opCode = packet.GetOpcode();
            switch (opCode)
            {
                case SMSG_PRE_RESURRECT: // No idea
                case CMSG_HEARTH_AND_RESURRECT: // No idea
                case SMSG_RESURRECT_REQUEST: // No idea, maybe never executed here because you can't send resurrect to anybody while dead
                case CMSG_RECLAIM_CORPSE: // Reviving going to corpse
                case CMSG_RESURRECT_RESPONSE: // Someone reviving your corpse
                case CMSG_SPIRIT_HEALER_ACTIVATE: // Spirit talking
                case SMSG_SPIRIT_HEALER_CONFIRM: // Spirit reviving, keep both spirit packets
                    ChatHandler(player->GetSession()).PSendSysMessage("You can't get resurrected. Travel as a ghost or create a new Hero.");
                    return false;
                case CMSG_GM_RESURRECT:
                    if (!sConfigMgr->GetOption<bool>("ModHardcoreGMCanResurrect.Enable", false))
                    {
                        return false;
                    }
                    break;
            }
        }

        return true;
    }
};


void AddSC_mod_harcore()
{
    new HardcoreMode();
    new HardModeServerScript();
}
