#include <engine/shared/protocolglue.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/translation_context.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol7.h>

int CGameClient::TranslateSnap(CSnapshot *pSnapDstSix, CSnapshot *pSnapSrcSeven, int Conn, bool Dummy)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	float LocalTime = Client()->LocalTime();
	int GameTick = Client()->GameTick(g_Config.m_ClDummy);
	CTranslationContext &TranslationContext = Client()->m_TranslationContext;

	for(auto &PlayerInfosRace : TranslationContext.m_apPlayerInfosRace)
		PlayerInfosRace = nullptr;

	int SpectatorId = -3;

	for(int i = 0; i < pSnapSrcSeven->NumItems(); i++)
	{
		const CSnapshotItem *pItem7 = (CSnapshotItem *)pSnapSrcSeven->GetItem(i);
		const int Size = pSnapSrcSeven->GetItemSize(i);
		const int ItemType = pSnapSrcSeven->GetItemType(i);

		// ddnet ex snap items
		if((ItemType > __NETOBJTYPE_UUID_HELPER && ItemType < OFFSET_NETMSGTYPE_UUID) || pItem7->Type() == NETOBJTYPE_EX)
		{
			CUnpacker Unpacker;
			Unpacker.Reset(pItem7->Data(), Size);

			void *pRawObj = GetNetObjHandler()->SecureUnpackObj(ItemType, &Unpacker);
			if(!pRawObj)
			{
				if(ItemType != UUID_UNKNOWN)
					dbg_msg("sixup", "dropped weird ddnet ex object '%s' (%d), failed on '%s'", GetNetObjHandler()->GetObjName(ItemType), ItemType, GetNetObjHandler()->FailedObjOn());
				continue;
			}
			const int ItemSize = GetNetObjHandler()->GetUnpackedObjSize(ItemType);

			void *pObj = Builder.NewItem(pItem7->Type(), pItem7->Id(), ItemSize);
			if(!pObj)
				return -17;

			mem_copy(pObj, pRawObj, ItemSize);
			continue;
		}

		if(GetNetObjHandler7()->ValidateObj(pItem7->Type(), pItem7->Data(), Size) != 0)
		{
			if(pItem7->Type() > 0 && pItem7->Type() < CSnapshot::OFFSET_UUID_TYPE)
			{
				dbg_msg(
					"sixup",
					"invalidated index=%d type=%d (%s) size=%d id=%d",
					i,
					pItem7->Type(),
					GetNetObjHandler7()->GetObjName(pItem7->Type()),
					Size,
					pItem7->Id());
			}
			pSnapSrcSeven->InvalidateItem(i);
		}

		if(pItem7->Type() == protocol7::NETOBJTYPE_PLAYERINFORACE)
		{
			const protocol7::CNetObj_PlayerInfoRace *pInfo = (const protocol7::CNetObj_PlayerInfoRace *)pItem7->Data();
			int ClientId = pItem7->Id();
			if(ClientId < MAX_CLIENTS && TranslationContext.m_aClients[ClientId].m_Active)
			{
				TranslationContext.m_apPlayerInfosRace[ClientId] = pInfo;
			}
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_SPECTATORINFO)
		{
			const protocol7::CNetObj_SpectatorInfo *pSpec7 = (const protocol7::CNetObj_SpectatorInfo *)pItem7->Data();
			SpectatorId = pSpec7->m_SpectatorId;
			if(pSpec7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				SpectatorId = SPEC_FREEVIEW;
		}
	}

	// hack to put game info in the snap
	// even though in 0.7 we get it as a game message
	// this message has to be on the top
	// the client looks at the items in order and needs the gameinfo at the top
	// otherwise it will not render skins with team colors
	if(TranslationContext.m_ShouldSendGameInfo)
	{
		void *pObj = Builder.NewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
		if(!pObj)
			return -1;

		int GameStateFlagsSix = 0;
		if(TranslationContext.m_GameStateFlags7 & protocol7::GAMESTATEFLAG_GAMEOVER)
			GameStateFlagsSix |= GAMESTATEFLAG_GAMEOVER;
		if(TranslationContext.m_GameStateFlags7 & protocol7::GAMESTATEFLAG_SUDDENDEATH)
			GameStateFlagsSix |= GAMESTATEFLAG_SUDDENDEATH;
		if(TranslationContext.m_GameStateFlags7 & protocol7::GAMESTATEFLAG_PAUSED)
			GameStateFlagsSix |= GAMESTATEFLAG_PAUSED;

		/*
			This is a 0.7 only flag that we just ignore for now

			GAMESTATEFLAG_ROUNDOVER
		*/

		CNetObj_GameInfo Info6 = {};
		Info6.m_GameFlags = TranslationContext.m_GameFlags;
		Info6.m_GameStateFlags = GameStateFlagsSix;
		Info6.m_RoundStartTick = TranslationContext.m_GameStartTick7;
		Info6.m_WarmupTimer = 0; // removed in 0.7
		if(TranslationContext.m_GameStateFlags7 & protocol7::GAMESTATEFLAG_WARMUP)
			Info6.m_WarmupTimer = TranslationContext.m_GameStateEndTick7 - GameTick;
		if(TranslationContext.m_GameStateFlags7 & protocol7::GAMESTATEFLAG_STARTCOUNTDOWN)
			Info6.m_WarmupTimer = TranslationContext.m_GameStateEndTick7 - GameTick;

		// hack to port 0.7 race timer to ddnet warmup gametimer hack
		int TimerClientId = clamp(TranslationContext.m_aLocalClientId[Conn], 0, (int)MAX_CLIENTS);
		if(SpectatorId >= 0)
			TimerClientId = SpectatorId;
		const protocol7::CNetObj_PlayerInfoRace *pRaceInfo = TranslationContext.m_apPlayerInfosRace[TimerClientId];
		if(pRaceInfo)
		{
			Info6.m_WarmupTimer = -pRaceInfo->m_RaceStartTick;
			Info6.m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
		}

		Info6.m_ScoreLimit = TranslationContext.m_ScoreLimit;
		Info6.m_TimeLimit = TranslationContext.m_TimeLimit;
		Info6.m_RoundNum = TranslationContext.m_MatchNum;
		Info6.m_RoundCurrent = TranslationContext.m_MatchCurrent;

		mem_copy(pObj, &Info6, sizeof(CNetObj_GameInfo));
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CTranslationContext::CClientData &Client = TranslationContext.m_aClients[i];
		if(!Client.m_Active)
			continue;

		void *pObj = Builder.NewItem(NETOBJTYPE_CLIENTINFO, i, sizeof(CNetObj_ClientInfo));
		if(!pObj)
			return -2;

		CNetObj_ClientInfo Info6 = {};
		StrToInts(&Info6.m_Name0, 4, Client.m_aName);
		StrToInts(&Info6.m_Clan0, 3, Client.m_aClan);
		Info6.m_Country = Client.m_Country;
		StrToInts(&Info6.m_Skin0, 6, Client.m_aSkinName);
		Info6.m_UseCustomColor = Client.m_UseCustomColor;
		Info6.m_ColorBody = Client.m_ColorBody;
		Info6.m_ColorFeet = Client.m_ColorFeet;
		mem_copy(pObj, &Info6, sizeof(CNetObj_ClientInfo));
	}

	bool NewGameData = false;

	for(int i = 0; i < pSnapSrcSeven->NumItems(); i++)
	{
		const CSnapshotItem *pItem7 = pSnapSrcSeven->GetItem(i);
		const int Size = pSnapSrcSeven->GetItemSize(i);
		// the first few items are a full match
		// no translation needed
		if(pItem7->Type() == protocol7::NETOBJTYPE_PROJECTILE ||
			pItem7->Type() == protocol7::NETOBJTYPE_LASER ||
			pItem7->Type() == protocol7::NETOBJTYPE_FLAG)
		{
			void *pObj = Builder.NewItem(pItem7->Type(), pItem7->Id(), Size);
			if(!pObj)
				return -4;

			mem_copy(pObj, pItem7->Data(), Size);
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_PICKUP)
		{
			void *pObj = Builder.NewItem(NETOBJTYPE_PICKUP, pItem7->Id(), sizeof(CNetObj_Pickup));
			if(!pObj)
				return -5;

			const protocol7::CNetObj_Pickup *pPickup7 = (const protocol7::CNetObj_Pickup *)pItem7->Data();
			CNetObj_Pickup Pickup6 = {};
			Pickup6.m_X = pPickup7->m_X;
			Pickup6.m_Y = pPickup7->m_Y;
			PickupType_SevenToSix(pPickup7->m_Type, Pickup6.m_Type, Pickup6.m_Subtype);

			mem_copy(pObj, &Pickup6, sizeof(CNetObj_Pickup));
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_GAMEDATA)
		{
			const protocol7::CNetObj_GameData *pGameData = (const protocol7::CNetObj_GameData *)pItem7->Data();
			TranslationContext.m_GameStateFlags7 = pGameData->m_GameStateFlags;
			TranslationContext.m_GameStartTick7 = pGameData->m_GameStartTick;
			TranslationContext.m_GameStateEndTick7 = pGameData->m_GameStateEndTick;
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_GAMEDATATEAM)
		{
			// 0.7 added GameDataTeam and GameDataFlag
			// both items merged together have all fields of the 0.6 GameData
			// so if we get either one of them we store the details in the translation context
			// and build one GameData snap item after this loop
			const protocol7::CNetObj_GameDataTeam *pTeam7 = (const protocol7::CNetObj_GameDataTeam *)pItem7->Data();

			TranslationContext.m_TeamscoreRed = pTeam7->m_TeamscoreRed;
			TranslationContext.m_TeamscoreBlue = pTeam7->m_TeamscoreBlue;
			NewGameData = true;
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_GAMEDATAFLAG)
		{
			const protocol7::CNetObj_GameDataFlag *pFlag7 = (const protocol7::CNetObj_GameDataFlag *)pItem7->Data();

			TranslationContext.m_FlagCarrierRed = pFlag7->m_FlagCarrierRed;
			TranslationContext.m_FlagCarrierBlue = pFlag7->m_FlagCarrierBlue;
			NewGameData = true;

			// used for blinking the flags in the hud
			// but that already works the 0.6 way
			// and is covered by the NETOBJTYPE_GAMEDATA translation
			// pFlag7->m_FlagDropTickRed;
			// pFlag7->m_FlagDropTickBlue;
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_CHARACTER)
		{
			void *pObj = Builder.NewItem(NETOBJTYPE_CHARACTER, pItem7->Id(), sizeof(CNetObj_Character));
			if(!pObj)
				return -6;

			const protocol7::CNetObj_Character *pChar7 = (const protocol7::CNetObj_Character *)pItem7->Data();

			CNetObj_Character Char6 = {};
			// character core is unchanged
			mem_copy(&Char6, pItem7->Data(), sizeof(CNetObj_CharacterCore));

			Char6.m_PlayerFlags = 0;
			if(pItem7->Id() >= 0 && pItem7->Id() < MAX_CLIENTS)
				Char6.m_PlayerFlags = PlayerFlags_SevenToSix(TranslationContext.m_aClients[pItem7->Id()].m_PlayerFlags7);
			Char6.m_Health = pChar7->m_Health;
			Char6.m_Armor = pChar7->m_Armor;
			Char6.m_AmmoCount = pChar7->m_AmmoCount;
			Char6.m_Weapon = pChar7->m_Weapon;
			Char6.m_Emote = pChar7->m_Emote;
			Char6.m_AttackTick = pChar7->m_AttackTick;

			if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_ATTACH_PLAYER)
			{
				void *pEvent = Builder.NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), sizeof(CNetEvent_SoundWorld));
				if(!pEvent)
					return -7;

				CNetEvent_SoundWorld Sound = {};
				Sound.m_X = pChar7->m_X;
				Sound.m_Y = pChar7->m_Y;
				Sound.m_SoundId = SOUND_HOOK_ATTACH_PLAYER;
				mem_copy(pEvent, &Sound, sizeof(CNetEvent_SoundWorld));
			}

			if(TranslationContext.m_aLocalClientId[Conn] != pItem7->Id())
			{
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_GROUND_JUMP)
				{
					void *pEvent = Builder.NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), sizeof(CNetEvent_SoundWorld));
					if(!pEvent)
						return -7;

					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_PLAYER_JUMP;
					mem_copy(pEvent, &Sound, sizeof(CNetEvent_SoundWorld));
				}
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_ATTACH_GROUND)
				{
					void *pEvent = Builder.NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), sizeof(CNetEvent_SoundWorld));
					if(!pEvent)
						return -7;

					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_HOOK_ATTACH_GROUND;
					mem_copy(pEvent, &Sound, sizeof(CNetEvent_SoundWorld));
				}
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_HIT_NOHOOK)
				{
					void *pEvent = Builder.NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), sizeof(CNetEvent_SoundWorld));
					if(!pEvent)
						return -7;

					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_HOOK_NOATTACH;
					mem_copy(pEvent, &Sound, sizeof(CNetEvent_SoundWorld));
				}
			}

			mem_copy(pObj, &Char6, sizeof(CNetObj_Character));
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_PLAYERINFO)
		{
			void *pObj = Builder.NewItem(NETOBJTYPE_PLAYERINFO, pItem7->Id(), sizeof(CNetObj_PlayerInfo));
			if(!pObj)
				return -8;

			const protocol7::CNetObj_PlayerInfo *pInfo7 = (const protocol7::CNetObj_PlayerInfo *)pItem7->Data();
			CNetObj_PlayerInfo Info6 = {};
			Info6.m_Local = TranslationContext.m_aLocalClientId[Conn] == pItem7->Id();
			Info6.m_ClientId = pItem7->Id();
			Info6.m_Team = 0;
			if(pItem7->Id() >= 0 && pItem7->Id() < MAX_CLIENTS)
			{
				Info6.m_Team = TranslationContext.m_aClients[pItem7->Id()].m_Team;
				TranslationContext.m_aClients[pItem7->Id()].m_PlayerFlags7 = pInfo7->m_PlayerFlags;
			}
			Info6.m_Score = pInfo7->m_Score;
			Info6.m_Latency = pInfo7->m_Latency;
			mem_copy(pObj, &Info6, sizeof(CNetObj_PlayerInfo));
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_SPECTATORINFO)
		{
			void *pObj = Builder.NewItem(NETOBJTYPE_SPECTATORINFO, pItem7->Id(), sizeof(CNetObj_SpectatorInfo));
			if(!pObj)
				return -9;

			const protocol7::CNetObj_SpectatorInfo *pSpec7 = (const protocol7::CNetObj_SpectatorInfo *)pItem7->Data();
			CNetObj_SpectatorInfo Spec6 = {};
			Spec6.m_SpectatorId = pSpec7->m_SpectatorId;
			if(pSpec7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				Spec6.m_SpectatorId = SPEC_FREEVIEW;
			Spec6.m_X = pSpec7->m_X;
			Spec6.m_Y = pSpec7->m_Y;
			mem_copy(pObj, &Spec6, sizeof(CNetObj_SpectatorInfo));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_EXPLOSION)
		{
			void *pEvent = Builder.NewItem(NETEVENTTYPE_EXPLOSION, pItem7->Id(), sizeof(CNetEvent_Explosion));
			if(!pEvent)
				return -10;

			const protocol7::CNetEvent_Explosion *pExplosion7 = (const protocol7::CNetEvent_Explosion *)pItem7->Data();
			CNetEvent_Explosion Explosion6 = {};
			Explosion6.m_X = pExplosion7->m_X;
			Explosion6.m_Y = pExplosion7->m_Y;
			mem_copy(pEvent, &Explosion6, sizeof(CNetEvent_Explosion));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_SPAWN)
		{
			void *pEvent = Builder.NewItem(NETEVENTTYPE_SPAWN, pItem7->Id(), sizeof(CNetEvent_Spawn));
			if(!pEvent)
				return -11;

			const protocol7::CNetEvent_Spawn *pSpawn7 = (const protocol7::CNetEvent_Spawn *)pItem7->Data();
			CNetEvent_Spawn Spawn6 = {};
			Spawn6.m_X = pSpawn7->m_X;
			Spawn6.m_Y = pSpawn7->m_Y;
			mem_copy(pEvent, &Spawn6, sizeof(CNetEvent_Spawn));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_HAMMERHIT)
		{
			void *pEvent = Builder.NewItem(NETEVENTTYPE_HAMMERHIT, pItem7->Id(), sizeof(CNetEvent_HammerHit));
			if(!pEvent)
				return -12;

			const protocol7::CNetEvent_HammerHit *pHammerHit7 = (const protocol7::CNetEvent_HammerHit *)pItem7->Data();
			CNetEvent_HammerHit HammerHit6 = {};
			HammerHit6.m_X = pHammerHit7->m_X;
			HammerHit6.m_Y = pHammerHit7->m_Y;
			mem_copy(pEvent, &HammerHit6, sizeof(CNetEvent_HammerHit));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_DEATH)
		{
			void *pEvent = Builder.NewItem(NETEVENTTYPE_DEATH, pItem7->Id(), sizeof(CNetEvent_Death));
			if(!pEvent)
				return -13;

			const protocol7::CNetEvent_Death *pDeath7 = (const protocol7::CNetEvent_Death *)pItem7->Data();
			CNetEvent_Death Death6 = {};
			Death6.m_X = pDeath7->m_X;
			Death6.m_Y = pDeath7->m_Y;
			Death6.m_ClientId = pDeath7->m_ClientId;
			mem_copy(pEvent, &Death6, sizeof(CNetEvent_Death));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_SOUNDWORLD)
		{
			void *pEvent = Builder.NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), sizeof(CNetEvent_SoundWorld));
			if(!pEvent)
				return -14;

			const protocol7::CNetEvent_SoundWorld *pSoundWorld7 = (const protocol7::CNetEvent_SoundWorld *)pItem7->Data();
			CNetEvent_SoundWorld SoundWorld6 = {};
			SoundWorld6.m_X = pSoundWorld7->m_X;
			SoundWorld6.m_Y = pSoundWorld7->m_Y;
			SoundWorld6.m_SoundId = pSoundWorld7->m_SoundId;
			mem_copy(pEvent, &SoundWorld6, sizeof(CNetEvent_SoundWorld));
		}
		else if(pItem7->Type() == protocol7::NETEVENTTYPE_DAMAGE)
		{
			// 0.7 introduced amount for damage indicators
			// so for one 0.7 item we might create multiple 0.6 ones
			const protocol7::CNetEvent_Damage *pDmg7 = (const protocol7::CNetEvent_Damage *)pItem7->Data();

			int Amount = pDmg7->m_HealthAmount + pDmg7->m_ArmorAmount;
			if(Amount < 1)
				continue;

			int ClientId = pDmg7->m_ClientId;
			TranslationContext.m_aDamageTaken[ClientId]++;

			float Angle;
			// create healthmod indicator
			if(LocalTime < TranslationContext.m_aDamageTakenTick[ClientId] + 0.5f)
			{
				// make sure that the damage indicators don't group together
				Angle = TranslationContext.m_aDamageTaken[ClientId] * 0.25f;
			}
			else
			{
				TranslationContext.m_aDamageTaken[ClientId] = 0;
				Angle = 0;
			}

			TranslationContext.m_aDamageTakenTick[ClientId] = LocalTime;

			float a = 3 * pi / 2 + Angle;
			float s = a - pi / 3;
			float e = a + pi / 3;
			for(int k = 0; k < Amount; k++)
			{
				// pItem7->Id() is reused that is technically wrong
				// but the client implementation does not look at the ids
				// and renders the damage indicators just fine
				void *pEvent = Builder.NewItem(NETEVENTTYPE_DAMAGEIND, pItem7->Id(), sizeof(CNetEvent_DamageInd));
				if(!pEvent)
					return -16;

				CNetEvent_DamageInd Dmg6 = {};
				Dmg6.m_X = pDmg7->m_X;
				Dmg6.m_Y = pDmg7->m_Y;
				float f = mix(s, e, float(k + 1) / float(Amount + 2));
				Dmg6.m_Angle = (int)(f * 256.0f);
				mem_copy(pEvent, &Dmg6, sizeof(CNetEvent_DamageInd));
			}
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_DE_CLIENTINFO)
		{
			const protocol7::CNetObj_De_ClientInfo *pInfo = (const protocol7::CNetObj_De_ClientInfo *)pItem7->Data();

			const int ClientId = pItem7->Id();

			if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			{
				dbg_msg("sixup", "De_ClientInfo got invalid ClientId: %d", ClientId);
				return -17;
			}

			if(pInfo->m_Local)
			{
				TranslationContext.m_aLocalClientId[Conn] = ClientId;
			}
			CTranslationContext::CClientData &Client = TranslationContext.m_aClients[ClientId];
			Client.m_Active = true;
			Client.m_Team = pInfo->m_Team;
			IntsToStr(pInfo->m_aName, 4, Client.m_aName, std::size(Client.m_aName));
			IntsToStr(pInfo->m_aClan, 3, Client.m_aClan, std::size(Client.m_aClan));
			Client.m_Country = pInfo->m_Country;

			ApplySkin7InfoFromSnapObj(pInfo, ClientId);
		}
		else if(pItem7->Type() == protocol7::NETOBJTYPE_DE_GAMEINFO)
		{
			const protocol7::CNetObj_De_GameInfo *pInfo = (const protocol7::CNetObj_De_GameInfo *)pItem7->Data();

			TranslationContext.m_GameFlags = pInfo->m_GameFlags;
			TranslationContext.m_ScoreLimit = pInfo->m_ScoreLimit;
			TranslationContext.m_TimeLimit = pInfo->m_TimeLimit;
			TranslationContext.m_MatchNum = pInfo->m_MatchNum;
			TranslationContext.m_MatchCurrent = pInfo->m_MatchCurrent;
			TranslationContext.m_ShouldSendGameInfo = true;
		}
	}

	if(NewGameData)
	{
		void *pObj = Builder.NewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
		if(!pObj)
			return -17;

		CNetObj_GameData GameData = {};
		GameData.m_TeamscoreRed = TranslationContext.m_TeamscoreRed;
		GameData.m_TeamscoreBlue = TranslationContext.m_TeamscoreBlue;
		GameData.m_FlagCarrierRed = TranslationContext.m_FlagCarrierRed;
		GameData.m_FlagCarrierBlue = TranslationContext.m_FlagCarrierBlue;
		mem_copy(pObj, &GameData, sizeof(CNetObj_GameData));
	}

	return Builder.Finish(pSnapDstSix);
}

int CGameClient::OnDemoRecSnap7(CSnapshot *pFrom, CSnapshot *pTo, int Conn)
{
	CSnapshotBuilder Builder;
	Builder.Init7(pFrom);

	// add client info
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_aClients[i].m_Active)
			continue;

		void *pItem = Builder.NewItem(protocol7::NETOBJTYPE_DE_CLIENTINFO, i, sizeof(protocol7::CNetObj_De_ClientInfo));
		if(!pItem)
			return -1;

		CTranslationContext::CClientData &ClientData = Client()->m_TranslationContext.m_aClients[i];

		protocol7::CNetObj_De_ClientInfo ClientInfoObj;
		ClientInfoObj.m_Local = i == Client()->m_TranslationContext.m_aLocalClientId[Conn];
		ClientInfoObj.m_Team = ClientData.m_Team;
		StrToInts(ClientInfoObj.m_aName, 4, m_aClients[i].m_aName);
		StrToInts(ClientInfoObj.m_aClan, 3, m_aClients[i].m_aClan);
		ClientInfoObj.m_Country = ClientData.m_Country;

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			StrToInts(ClientInfoObj.m_aaSkinPartNames[Part], 6, m_aClients[i].m_aSixup[Conn].m_aaSkinPartNames[Part]);
			ClientInfoObj.m_aUseCustomColors[Part] = m_aClients[i].m_aSixup[Conn].m_aUseCustomColors[Part];
			ClientInfoObj.m_aSkinPartColors[Part] = m_aClients[i].m_aSixup[Conn].m_aSkinPartColors[Part];
		}

		mem_copy(pItem, &ClientInfoObj, sizeof(protocol7::CNetObj_De_ClientInfo));
	}

	// add tuning
	CTuningParams StandardTuning;
	if(mem_comp(&StandardTuning, &m_aTuning[Conn], sizeof(CTuningParams)) != 0)
	{
		void *pItem = Builder.NewItem(protocol7::NETOBJTYPE_DE_TUNEPARAMS, 0, sizeof(protocol7::CNetObj_De_TuneParams));
		if(!pItem)
			return -2;

		protocol7::CNetObj_De_TuneParams TuneParams;
		mem_copy(&TuneParams.m_aTuneParams, &m_aTuning[Conn], sizeof(TuneParams));
		mem_copy(pItem, &TuneParams, sizeof(protocol7::CNetObj_De_TuneParams));
	}

	// add game info
	void *pItem = Builder.NewItem(protocol7::NETOBJTYPE_DE_GAMEINFO, 0, sizeof(protocol7::CNetObj_De_GameInfo));
	if(!pItem)
		return -3;

	protocol7::CNetObj_De_GameInfo GameInfo;

	GameInfo.m_GameFlags = Client()->m_TranslationContext.m_GameFlags;
	GameInfo.m_ScoreLimit = Client()->m_TranslationContext.m_ScoreLimit;
	GameInfo.m_TimeLimit = Client()->m_TranslationContext.m_TimeLimit;
	GameInfo.m_MatchNum = Client()->m_TranslationContext.m_MatchNum;
	GameInfo.m_MatchCurrent = Client()->m_TranslationContext.m_MatchCurrent;

	mem_copy(pItem, &GameInfo, sizeof(protocol7::CNetObj_De_GameInfo));

	return Builder.Finish(pTo);
}
