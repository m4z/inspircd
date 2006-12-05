/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Allows for hashed oper passwords */
/* $ModDep: m_hash.h */

using namespace std;

#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

#include "m_hash.h"

enum ProviderTypes
{
	PROV_MD5 = 1,
	PROV_SHA = 2
};

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* MD5Provider;
	Module* SHAProvider;
	Module* Sender;
	int Prov;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* Sender, Module* MD5Hasher, Module* SHAHasher, int P)
		: command_t(Instance,"MKPASSWD", 'o', 2), MD5Provider(MD5Hasher), SHAProvider(SHAHasher), Prov(P)
	{
		this->source = "m_oper_hash.so";
		syntax = "<hashtype> <any-text>";
	}

	void MakeHash(userrec* user, Module* ProviderMod, const char* algo, const char* stuff)
	{
		HashResetRequest(Sender, ProviderMod).Send();
		user->WriteServ("NOTICE %s :%s hashed password for %s is %s",user->nick, algo, stuff, HashSumRequest(Sender, ProviderMod, stuff).Send() );
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if ((!strcasecmp(parameters[0], "MD5")) && ((Prov & PROV_MD5) > 0))
		{
			MakeHash(user, MD5Provider, "MD5", parameters[1]);
		}
		else if ((!strcasecmp(parameters[0], "SHA256")) && ((Prov & PROV_SHA) > 0))
		{
			MakeHash(user, SHAProvider, "SHA256", parameters[1]);
		}
		else
		{
			user->WriteServ("NOTICE %s :Unknown hash type, valid hash types are:%s%s", user->nick, ((Prov & PROV_MD5) > 0) ? " MD5" : "", ((Prov & PROV_SHA) > 0) ? " SHA256" : "");
		}

		/* NOTE: Don't propogate this across the network!
		 * We dont want plaintext passes going all over the place...
		 * To make sure it goes nowhere, return CMD_FAILURE!
		 */
		return CMD_FAILURE;
	}
};

class ModuleOperHash : public Module
{
	
	cmd_mkpasswd* mycommand;
	Module* MD5Provider;
	Module* SHAProvider;
	std::string providername;
	int ID;
	ConfigReader* Conf;

	std::map<std::string, Module*> hashers;

 public:

	ModuleOperHash(InspIRCd* Me)
		: Module::Module(Me)
	{
		ID = 0;
		Conf = NULL;
		OnRehash("");

		modulelist* ml = ServerInstance->FindInterface("HashRequest");

		if (ml)
		{
			ServerInstance->Log(DEBUG, "Found interface 'HashRequest' containing %d modules", ml->size());

			for (modulelist::iterator m = ml->begin(); m != ml->end(); ml++)
			{
				std::string name = HashNameRequest(this, *m).Send();
				hashers[name] = *m;
				ServerInstance->Log(DEBUG, "Found HashRequest interface: '%s' -> '%08x'", name.c_str(), *m);
			}
		}

		/* Try to find the md5 service provider, bail if it can't be found */
		MD5Provider = ServerInstance->FindModule("m_md5.so");
		if (MD5Provider)
			ID |= PROV_MD5;

		SHAProvider = ServerInstance->FindModule("m_sha256.so");
		if (SHAProvider)
			ID |= PROV_SHA;

		mycommand = new cmd_mkpasswd(ServerInstance, this, MD5Provider, SHAProvider, ID);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperHash()
	{
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnOperCompare] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		if (Conf)
			delete Conf;

		Conf = new ConfigReader(ServerInstance);
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input, int tagnumber)
	{
		std::string hashtype = Conf->ReadValue("oper", "hash", tagnumber);
		Module* ModPtr = NULL;

		if ((hashtype == "sha256") && (data.length() == SHA256_BLOCK_SIZE) && ((ID & PROV_SHA) > 0))
		{
			ModPtr = SHAProvider;
		}
		else if ((hashtype == "md5") && (data.length() == 32) && ((ID & PROV_MD5) > 0))
		{
			ModPtr = MD5Provider;
		}
		if (ModPtr)
		{
			HashResetRequest(this, ModPtr).Send();
			if (!strcasecmp(data.c_str(), HashSumRequest(this, ModPtr, input.c_str()).Send()))
				return 1;
			else return -1;
		}

		return 0;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


class ModuleOperHashFactory : public ModuleFactory
{
 public:
	ModuleOperHashFactory()
	{
	}
	
	~ModuleOperHashFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperHash(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperHashFactory;
}
