// chillerbot-ux reply to ping

#include "game/client/gameclient.h"

#include "replytoping.h"

CLangParser &CReplyToPing::LangParser() { return ChatHelper()->LangParser(); }


CReplyToPing::CReplyToPing(CChatHelper *pChatHelper)
{
    m_pChatHelper = pChatHelper;
}

bool CReplyToPing::ReplyToLastPing(const char *pMessageAuthor, const char *pMessageAuthorClan, const char *pMessage, char *pResponse, int SizeOfResponse)
{
	if(!pResponse)
		return false;
	pResponse[0] = '\0';
	if(pMessageAuthor[0] == '\0')
		return false;
	if(pMessage[0] == '\0')
		return false;

	int MsgLen = str_length(pMessage);
	int NameLen = 0;
	const char *pName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aName;
	const char *pDummyName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aName;
	const char *pClan = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aClan;
	const char *pDummyClan = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aClan;

	if(ChatHelper()->LineShouldHighlight(pMessage, pName))
		NameLen = str_length(pName);
	else if(ChatHelper()->GameClient()->Client()->DummyConnected() && ChatHelper()->LineShouldHighlight(pMessage, pDummyName))
		NameLen = str_length(pDummyName);

	// ping without further context
	if(MsgLen < NameLen + 2)
	{
		str_format(pResponse, SizeOfResponse, "%s ?", pMessageAuthor);
		return true;
	}
	// greetings
	if(LangParser().IsGreeting(pMessage))
	{
		str_format(pResponse, SizeOfResponse, "hi %s", pMessageAuthor);
		return true;
	}
	if(LangParser().IsBye(pMessage))
	{
		str_format(pResponse, SizeOfResponse, "bye %s", pMessageAuthor);
		return true;
	}
	// can i join your clan?
	if(str_find_nocase(pMessage, "clan") &&
		(str_find_nocase(pMessage, "enter") ||
			str_find_nocase(pMessage, "join") ||
			str_find_nocase(pMessage, "let me") ||
			str_find_nocase(pMessage, "beitreten") ||
			str_find_nocase(pMessage, " in ") ||
			str_find_nocase(pMessage, "can i") ||
			str_find_nocase(pMessage, "can me") ||
			str_find_nocase(pMessage, "me you") ||
			str_find_nocase(pMessage, "me is") ||
			str_find_nocase(pMessage, "into")))
	{
		char aResponse[1024];
		if(ChatHelper()->HowToJoinClan(pClan, aResponse, sizeof(aResponse)) || (ChatHelper()->GameClient()->Client()->DummyConnected() && ChatHelper()->HowToJoinClan(pDummyClan, aResponse, SizeOfResponse)))
		{
			str_format(pResponse, SizeOfResponse, "%s %s", pMessageAuthor, aResponse);
			return true;
		}
	}
	// check if a player has war or not
	// TODO:

	// check war reason for others
	const char *pWhy = str_find_nocase(pMessage, "why");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "stop");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do not");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "don't");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "do you");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "is u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "is you");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "are u");
	if(!pWhy)
		pWhy = str_find_nocase(pMessage, "are you");
	if(pWhy)
	{
		const char *pKill = NULL;
		pKill = str_find_nocase(pWhy, "kill "); // why do you kill foo?
		if(pKill)
			pKill = pKill + str_length("kill ");
		else if((pKill = str_find_nocase(pWhy, "kil "))) // why kil foo?
			pKill = pKill + str_length("kil ");
		else if((pKill = str_find_nocase(pWhy, "killed "))) // why killed foo?
			pKill = pKill + str_length("killed ");
		else if((pKill = str_find_nocase(pWhy, "block "))) // why do you block foo?
			pKill = pKill + str_length("block ");
		else if((pKill = str_find_nocase(pWhy, "blocked "))) // why do you blocked foo?
			pKill = pKill + str_length("blocked ");
		else if((pKill = str_find_nocase(pWhy, "help "))) // why no help foo?
			pKill = pKill + str_length("help ");
		else if((pKill = str_find_nocase(pWhy, "war with "))) // why do you have war with foo?
			pKill = pKill + str_length("war with ");
		else if((pKill = str_find_nocase(pWhy, "war "))) // why war foo?
			pKill = pKill + str_length("war ");
		else if((pKill = str_find_nocase(pWhy, "wared "))) // why wared foo?
			pKill = pKill + str_length("wared ");
		else if((pKill = str_find_nocase(pWhy, "waring "))) // why waring foo?
			pKill = pKill + str_length("waring ");
		else if((pKill = str_find_nocase(pWhy, "bully "))) // why bully foo?
			pKill = pKill + str_length("bully ");
		else if((pKill = str_find_nocase(pWhy, "bullying "))) // why bullying foo?
			pKill = pKill + str_length("bullying ");
		else if((pKill = str_find_nocase(pWhy, "bullied "))) // why bullied foo?
			pKill = pKill + str_length("bullied ");
		else if((pKill = str_find_nocase(pWhy, "freeze "))) // why freeze foo?
			pKill = pKill + str_length("freeze ");

		if(pKill)
		{
			bool HasWar = true;
			// aVictim also has to hold the full own name to match the chop off
			char aVictim[MAX_NAME_LENGTH + 3 + MAX_NAME_LENGTH];
			str_copy(aVictim, pKill, sizeof(aVictim));
			if(!ChatHelper()->GameClient()->m_WarList.IsWarlist(aVictim) && !ChatHelper()->GameClient()->m_WarList.IsTraitorlist(aVictim))
			{
				HasWar = false;
				while(str_endswith(aVictim, "?")) // cut off the question marks from the victim name
					aVictim[str_length(aVictim) - 1] = '\0';
				// cut off own name from the victime name if question in this format "why do you war foo (your name)"
				char aOwnName[MAX_NAME_LENGTH + 3];
				// main tee
				str_format(aOwnName, sizeof(aOwnName), " %s", ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aName);
				if(str_endswith_nocase(aVictim, aOwnName))
					aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
				if(ChatHelper()->GameClient()->Client()->DummyConnected())
				{
					str_format(aOwnName, sizeof(aOwnName), " %s", ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aName);
					if(str_endswith_nocase(aVictim, aOwnName))
						aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
				}

				// cut off descriptions like this
				// why do you block foo he is new here!
				// why do you block foo she is my friend!!
				for(int i = 0; i < str_length(aVictim); i++)
				{
					// c++ be like...
					if(i < 2)
						continue;
					if(aVictim[i - 1] != ' ')
						continue;
					if((aVictim[i] != 'h' || !aVictim[i + 1] || aVictim[i + 1] != 'e' || !aVictim[i + 2] || aVictim[i + 2] != ' ') &&
						(aVictim[i] != 's' || !aVictim[i + 1] || aVictim[i + 1] != 'h' || !aVictim[i + 2] || aVictim[i + 2] != 'e' || !aVictim[i + 3] || aVictim[i + 3] != ' '))
						continue;

					aVictim[i - 1] = '\0';
					break;
				}

				// do not kill my friend foo
				const char *pFriend = NULL;
				if((pFriend = str_find_nocase(aVictim, " friend ")))
					pFriend += str_length(" friend ");
				else if((pFriend = str_find_nocase(aVictim, " frint ")))
					pFriend += str_length(" frint ");
				else if((pFriend = str_find_nocase(aVictim, " mate ")))
					pFriend += str_length(" mate ");
				else if((pFriend = str_find_nocase(aVictim, " bff ")))
					pFriend += str_length(" bff ");
				else if((pFriend = str_find_nocase(aVictim, " girlfriend ")))
					pFriend += str_length(" girlfriend ");
				else if((pFriend = str_find_nocase(aVictim, " boyfriend ")))
					pFriend += str_length(" boyfriend ");
				else if((pFriend = str_find_nocase(aVictim, " dog ")))
					pFriend += str_length(" dog ");
				else if((pFriend = str_find_nocase(aVictim, " gf ")))
					pFriend += str_length(" gf ");
				else if((pFriend = str_find_nocase(aVictim, " bf ")))
					pFriend += str_length(" bf ");

				if(pFriend)
					str_copy(aVictim, pFriend, sizeof(aVictim));
			}

			char aWarReason[128];
			if(HasWar || ChatHelper()->GameClient()->m_WarList.IsWarlist(aVictim) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(aVictim))
			{
				ChatHelper()->GameClient()->m_WarList.GetWarReason(aVictim, aWarReason, sizeof(aWarReason));
				if(aWarReason[0])
					str_format(pResponse, SizeOfResponse, "%s: %s has war because: %s", pMessageAuthor, aVictim, aWarReason);
				else
					str_format(pResponse, SizeOfResponse, "%s: the name %s is on my warlist.", pMessageAuthor, aVictim);
				return true;
			}
			for(auto &Client : ChatHelper()->GameClient()->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(str_comp(Client.m_aName, aVictim))
					continue;

				if(ChatHelper()->GameClient()->m_WarList.IsWarClanlist(Client.m_aClan))
				{
					str_format(pResponse, SizeOfResponse, "%s i war %s because his clan %s is on my warlist.", pMessageAuthor, aVictim, Client.m_aClan);
					return true;
				}
			}
		}
	}
	// why? (check war for self)
	if(LangParser().IsQuestionWhy(pMessage) || (str_find(pMessage, "?") && MsgLen < NameLen + 4) ||
		((str_find(pMessage, "stop") || str_find_nocase(pMessage, "help")) && (ChatHelper()->GameClient()->m_WarList.IsWarlist(pMessageAuthor) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(pMessageAuthor))))
	{
		char aWarReason[128];
		if(ChatHelper()->GameClient()->m_WarList.IsWarlist(pMessageAuthor) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(pMessageAuthor))
		{
			ChatHelper()->GameClient()->m_WarList.GetWarReason(pMessageAuthor, aWarReason, sizeof(aWarReason));
			if(aWarReason[0])
				str_format(pResponse, SizeOfResponse, "%s has war because: %s", pMessageAuthor, aWarReason);
			else
				str_format(pResponse, SizeOfResponse, "%s you are on my warlist.", pMessageAuthor);
			return true;
		}
		else if(ChatHelper()->GameClient()->m_WarList.IsWarClanlist(pMessageAuthorClan))
		{
			str_format(pResponse, SizeOfResponse, "%s your clan is on my warlist.", pMessageAuthor);
			return true;
		}
	}

	// spec me
	if(str_find_nocase(pMessage, "spec") || str_find_nocase(pMessage, "watch") || (str_find_nocase(pMessage, "look") && !str_find_nocase(pMessage, "looks")) || str_find_nocase(pMessage, "schau"))
	{
		str_format(pResponse, SizeOfResponse, "/pause %s", pMessageAuthor);
		str_format(pResponse, SizeOfResponse, "%s ok i am watching you", pMessageAuthor);
		return true;
	}
	// wanna? (always say no automated if motivated to do something type yes manually)
	if(str_find_nocase(pMessage, "wanna") || str_find_nocase(pMessage, "want"))
	{
		// TODO: fix tone
		// If you get asked to be given something "no sorry" sounds weird
		// If you are being asked to do something together "no thanks" sounds weird
		// the generic "no" might be a bit dry
		str_format(pResponse, SizeOfResponse, "%s no", pMessageAuthor);
		return true;
	}
	// help
	if(str_find_nocase(pMessage, "help") || str_find_nocase(pMessage, "hilfe"))
	{
		str_format(pResponse, SizeOfResponse, "%s where? what?", pMessageAuthor);
		return true;
	}
	// small talk
	if(str_find_nocase(pMessage, "how are you") ||
		str_find_nocase(pMessage, "how r u") ||
		str_find_nocase(pMessage, "how ru ") ||
		str_find_nocase(pMessage, "how ru?") ||
		str_find_nocase(pMessage, "how r you") ||
		str_find_nocase(pMessage, "how are u") ||
		str_find_nocase(pMessage, "how is it going") ||
		str_find_nocase(pMessage, "ca va") ||
		(str_find_nocase(pMessage, "как") && str_find_nocase(pMessage, "дела")))
	{
		str_format(pResponse, SizeOfResponse, "%s good, and you? :)", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "wie gehts") || str_find_nocase(pMessage, "wie geht es") || str_find_nocase(pMessage, "was geht"))
	{
		str_format(pResponse, SizeOfResponse, "%s gut, und dir? :)", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "about you") || str_find_nocase(pMessage, "and you") || str_find_nocase(pMessage, "and u") ||
		(str_find_nocase(pMessage, "u?") && MsgLen < NameLen + 5) ||
		(str_find_nocase(pMessage, "wbu") && MsgLen < NameLen + 8) ||
		(str_find_nocase(pMessage, "hbu") && MsgLen < NameLen + 8))
	{
		str_format(pResponse, SizeOfResponse, "%s good", pMessageAuthor);
		return true;
	}
	// chillerbot-ux features
	if(LangParser().IsQuestionHow(pMessage))
	{
		// feature: auto_drop_money
		if(str_find_nocase(pMessage, "drop") && (str_find_nocase(pMessage, "money") || str_find_nocase(pMessage, "moni") || str_find_nocase(pMessage, "coin") || str_find_nocase(pMessage, "cash") || str_find_nocase(pMessage, "geld")))
		{
			str_format(pResponse, SizeOfResponse, "%s I auto drop money using \"auto_drop_money\" in chillerbot-ux", pMessageAuthor);
			return true;
		}
		// feature: auto reply
		if((str_find_nocase(pMessage, "reply") && str_find_nocase(pMessage, "chat")) || (str_find_nocase(pMessage, "auto chat") || str_find_nocase(pMessage, "autochat")) ||
			str_find_nocase(pMessage, "message") ||
			((str_find_nocase(pMessage, "fast") || str_find_nocase(pMessage, "quick")) && str_find_nocase(pMessage, "chat")))
		{
			str_format(pResponse, SizeOfResponse, "%s I bound the chillerbot-ux command \"reply_to_last_ping\" to automate chat", pMessageAuthor);
			return true;
		}
	}
	// advertise chillerbot
	if(str_find_nocase(pMessage, "what client") || str_find_nocase(pMessage, "which client") || str_find_nocase(pMessage, "wat client") ||
		str_find_nocase(pMessage, "good client") || str_find_nocase(pMessage, "download client") || str_find_nocase(pMessage, "link client") || str_find_nocase(pMessage, "get client") ||
		str_find_nocase(pMessage, "where chillerbot") || str_find_nocase(pMessage, "download chillerbot") || str_find_nocase(pMessage, "link chillerbot") || str_find_nocase(pMessage, "get chillerbot") ||
		str_find_nocase(pMessage, "chillerbot url") || str_find_nocase(pMessage, "chillerbot download") || str_find_nocase(pMessage, "chillerbot link") || str_find_nocase(pMessage, "chillerbot website") ||
		((str_find_nocase(pMessage, "ddnet") || str_find_nocase(pMessage, "vanilla")) && str_find_nocase(pMessage, "?")))
	{
		str_format(pResponse, SizeOfResponse, "%s I use chillerbot-ux ( https://chillerbot.github.io )", pMessageAuthor);
		return true;
	}
	// whats your setting (mousesense, distance, dyn)
	if((str_find_nocase(pMessage, "?") ||
		   str_find_nocase(pMessage, "what") ||
		   str_find_nocase(pMessage, "which") ||
		   str_find_nocase(pMessage, "wat") ||
		   str_find_nocase(pMessage, "much") ||
		   str_find_nocase(pMessage, "many") ||
		   str_find_nocase(pMessage, "viel") ||
		   str_find_nocase(pMessage, "hoch")) &&
		(str_find_nocase(pMessage, "sens") || str_find_nocase(pMessage, "sesn") || str_find_nocase(pMessage, "snse") || str_find_nocase(pMessage, "senes") || str_find_nocase(pMessage, "inp") || str_find_nocase(pMessage, "speed")))
	{
		str_format(pResponse, SizeOfResponse, "%s my current inp_mousesens is %d", pMessageAuthor, g_Config.m_InpMousesens);
		return true;
	}
	if((str_find_nocase(pMessage, "?") || str_find_nocase(pMessage, "what") || str_find_nocase(pMessage, "which") || str_find_nocase(pMessage, "wat") || str_find_nocase(pMessage, "much") || str_find_nocase(pMessage, "many")) &&
		str_find_nocase(pMessage, "dist"))
	{
		str_format(pResponse, SizeOfResponse, "%s my current cl_mouse_max_distance is %d", pMessageAuthor, g_Config.m_ClMouseMaxDistance);
		return true;
	}
	if((str_find_nocase(pMessage, "?") || str_find_nocase(pMessage, "do you") || str_find_nocase(pMessage, "do u")) &&
		str_find_nocase(pMessage, "dyn"))
	{
		str_format(pResponse, SizeOfResponse, "%s my dyncam is currently %s", pMessageAuthor, g_Config.m_ClDyncam ? "on" : "off");
		return true;
	}
	// compliments
	if(str_find_nocase(pMessage, "good") ||
		str_find_nocase(pMessage, "happy") ||
		str_find_nocase(pMessage, "congrats") ||
		str_find_nocase(pMessage, "nice") ||
		str_find_nocase(pMessage, "pro"))
	{
		str_format(pResponse, SizeOfResponse, "%s thanks", pMessageAuthor);
		return true;
	}
	// impatient
	if(str_find_nocase(pMessage, "answer") || str_find_nocase(pMessage, "ignore") || str_find_nocase(pMessage, "antwort") || str_find_nocase(pMessage, "ignorier"))
	{
		str_format(pResponse, SizeOfResponse, "%s i am currently busy (automated reply)", pMessageAuthor);
		return true;
	}
	// ask to ask
	if(LangParser().IsAskToAsk(pMessage, pMessageAuthor, pResponse, SizeOfResponse))
		return true;
	// got weapon?
	if(str_find_nocase(pMessage, "got") || str_find_nocase(pMessage, "have") || str_find_nocase(pMessage, "hast"))
	{
		int Weapon = -1;
		if(str_find_nocase(pMessage, "hammer"))
			Weapon = WEAPON_HAMMER;
		else if(str_find_nocase(pMessage, "gun"))
			Weapon = WEAPON_GUN;
		else if(str_find_nocase(pMessage, "sg") || str_find_nocase(pMessage, "shotgun") || str_find_nocase(pMessage, "shotty"))
			Weapon = WEAPON_SHOTGUN;
		else if(str_find_nocase(pMessage, "nade") || str_find_nocase(pMessage, "rocket") || str_find_nocase(pMessage, "bazooka"))
			Weapon = WEAPON_GRENADE;
		else if(str_find_nocase(pMessage, "rifle") || str_find_nocase(pMessage, "laser") || str_find_nocase(pMessage, "sniper"))
			Weapon = WEAPON_LASER;
		if(CCharacter *pChar = ChatHelper()->GameClient()->m_GameWorld.GetCharacterByID(ChatHelper()->GameClient()->m_LocalIDs[g_Config.m_ClDummy]))
		{
			char aWeapons[1024];
			aWeapons[0] = '\0';
			if(pChar->GetWeaponGot(WEAPON_HAMMER))
				str_append(aWeapons, "hammer", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GUN))
				str_append(aWeapons, aWeapons[0] ? ", gun" : "gun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_SHOTGUN))
				str_append(aWeapons, aWeapons[0] ? ", shotgun" : "shotgun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GRENADE))
				str_append(aWeapons, aWeapons[0] ? ", grenade" : "grenade", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_LASER))
				str_append(aWeapons, aWeapons[0] ? ", rifle" : "rifle", sizeof(aWeapons));

			if(pChar->GetWeaponGot(Weapon))
				str_format(pResponse, SizeOfResponse, "%s Yes I got those weapons: %s", pMessageAuthor, aWeapons);
			else
				str_format(pResponse, SizeOfResponse, "%s No I got those weapons: %s", pMessageAuthor, aWeapons);
			return true;
		}
	}
	// weeb
	if(str_find_nocase(pMessage, "uwu"))
	{
		str_format(pResponse, SizeOfResponse, "%s OwO", pMessageAuthor);
		return true;
	}
	if(str_find_nocase(pMessage, "owo"))
	{
		str_format(pResponse, SizeOfResponse, "%s UwU", pMessageAuthor);
		return true;
	}
	// no u
	if(MsgLen < NameLen + 8 && (str_find_nocase(pMessage, "no u") ||
					   str_find_nocase(pMessage, "no you") ||
					   str_find_nocase(pMessage, "noob") ||
					   str_find_nocase(pMessage, "nob") ||
					   str_find_nocase(pMessage, "nuub") ||
					   str_find_nocase(pMessage, "nub") ||
					   str_find_nocase(pMessage, "bad")))
	{
		str_format(pResponse, SizeOfResponse, "%s no u", pMessageAuthor);
		return true;
	}
	return false;
}
