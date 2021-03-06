/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/** Handles user mode +k
 */
class ServProtectMode : public ModeHandler
{
 public:
	ServProtectMode(Module* Creator) : ModeHandler(Creator, "servprotect", 'k', PARAM_NONE, MODETYPE_USER) { oper = true; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		/* Because this returns MODEACTION_DENY all the time, there is only ONE
		 * way to add this mode and that is at client introduction in the UID command,
		 * as this calls OnModeChange for each mode but disregards the return values.
		 * The mode cannot be manually added or removed, not even by a server or by a remote
		 * user or uline, which prevents its (ab)use as a kiddie 'god mode' on such networks.
		 * I'm sure if someone really wants to do that they can make a copy of this module
		 * that does the job. It won't be me though!
		 */
		return MODEACTION_DENY;
	}
};

class ModuleServProtectMode : public Module
{
	ServProtectMode bm;
 public:
	ModuleServProtectMode()
		: bm(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(bm);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides usermode +k to protect services from kicks, kills, and mode changes.", VF_VENDOR);
	}

	void OnWhois(User* user, User* dest) CXX11_OVERRIDE
	{
		if (dest->IsModeSet(bm))
		{
			ServerInstance->SendWhoisLine(user, dest, 310, user->nick+" "+dest->nick+" :is a Network Service on "+ServerInstance->Config->Network);
		}
	}

	ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt) CXX11_OVERRIDE
	{
		/* Check that the mode is not a server mode, it is being removed, the user making the change is local, there is a parameter,
		 * and the user making the change is not a uline
		 */
		if (!adding && chan && IS_LOCAL(user) && !param.empty() && !ServerInstance->ULine(user->server))
		{
			/* Check if the parameter is a valid nick/uuid
			 */
			User *u = ServerInstance->FindNick(param);
			if (u)
			{
				Membership* memb = chan->GetUser(u);
				/* The target user has +k set on themselves, and you are trying to remove a privilege mode the user has set on themselves.
				 * This includes any prefix permission mode, even those registered in other modules, e.g. +qaohv. Using ::ModeString()
				 * here means that the number of modes is restricted to only modes the user has, limiting it to as short a loop as possible.
				 */
				if (u->IsModeSet(bm) && memb && memb->modes.find(mode) != std::string::npos)
				{
					/* BZZZT, Denied! */
					user->WriteNumeric(482, "%s %s :You are not permitted to remove privileges from %s services", user->nick.c_str(), chan->name.c_str(), ServerInstance->Config->Network.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		/* Mode allowed */
		return MOD_RES_PASSTHRU;
	}

	ModResult OnKill(User* src, User* dst, const std::string &reason) CXX11_OVERRIDE
	{
		if (src == NULL)
			return MOD_RES_PASSTHRU;

		if (dst->IsModeSet(bm))
		{
			src->WriteNumeric(485, "%s :You are not permitted to kill %s services!", src->nick.c_str(), ServerInstance->Config->Network.c_str());
			ServerInstance->SNO->WriteGlobalSno('a', src->nick+" tried to kill service "+dst->nick+" ("+reason+")");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User *src, Membership* memb, const std::string &reason) CXX11_OVERRIDE
	{
		if (memb->user->IsModeSet(bm))
		{
			src->WriteNumeric(484, "%s %s :You are not permitted to kick services",
				src->nick.c_str(), memb->chan->name.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoisLine(User* src, User* dst, int &numeric, std::string &text) CXX11_OVERRIDE
	{
		return ((src != dst) && (numeric == 319) && dst->IsModeSet(bm)) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};


MODULE_INIT(ModuleServProtectMode)
