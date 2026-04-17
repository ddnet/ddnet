#include <engine/shared/protocolglue.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/translation_context.h>

#include <generated/protocol7.h>

#include <game/client/gameclient.h>

int CGameClient::TranslateSnap(CSnapshotBuffer *pSnapDstSix, CSnapshot *pSnapSrcSeven, int Conn, bool Dummy)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

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

		if(ItemType <= 0)
		{
			// Don't add extended item type descriptions, they get
			// added implicitly (== 0).
			//
			// Don't add items of unknown item types either (< 0).
			continue;
		}

		// ddnet ex snap items
		if(ItemType >= OFFSET_UUID)
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

			pBuilder->NewItem(ItemType, pItem7->Id(), rust::Slice((const int32_t *)pRawObj, ItemSize / sizeof(int32_t)));
			continue;
		}

		if(GetNetObjHandler7()->ValidateObj(ItemType, pItem7->Data(), Size) != 0)
		{
			dbg_msg(
				"sixup",
				"invalidated index=%d type=%d (%s) size=%d id=%d",
				i,
				ItemType,
				GetNetObjHandler7()->GetObjName(ItemType),
				Size,
				pItem7->Id());
			pSnapSrcSeven->InvalidateItem(i);
		}

		if(ItemType == protocol7::NETOBJTYPE_PLAYERINFORACE)
		{
			const protocol7::CNetObj_PlayerInfoRace *pInfo = (const protocol7::CNetObj_PlayerInfoRace *)pItem7->Data();
			int ClientId = pItem7->Id();
			if(ClientId < MAX_CLIENTS && TranslationContext.m_aClients[ClientId].m_Active)
			{
				TranslationContext.m_apPlayerInfosRace[ClientId] = pInfo;
			}
		}
		else if(ItemType == protocol7::NETOBJTYPE_SPECTATORINFO)
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
		int TimerClientId = TranslationContext.m_aLocalClientId[Conn];
		if(SpectatorId >= 0)
			TimerClientId = SpectatorId;
		const protocol7::CNetObj_PlayerInfoRace *pRaceInfo = TimerClientId == -1 ? nullptr : TranslationContext.m_apPlayerInfosRace[TimerClientId];
		if(pRaceInfo)
		{
			Info6.m_WarmupTimer = -pRaceInfo->m_RaceStartTick;
			Info6.m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
		}

		Info6.m_ScoreLimit = TranslationContext.m_ScoreLimit;
		Info6.m_TimeLimit = TranslationContext.m_TimeLimit;
		Info6.m_RoundNum = TranslationContext.m_MatchNum;
		Info6.m_RoundCurrent = TranslationContext.m_MatchCurrent;

		pBuilder->NewItem(NETOBJTYPE_GAMEINFO, 0, Info6.AsSlice());
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CTranslationContext::CClientData &Client = TranslationContext.m_aClients[i];
		if(!Client.m_Active)
			continue;

		CNetObj_ClientInfo Info6 = {};
		StrToInts(Info6.m_aName, std::size(Info6.m_aName), Client.m_aName);
		StrToInts(Info6.m_aClan, std::size(Info6.m_aClan), Client.m_aClan);
		Info6.m_Country = Client.m_Country;
		StrToInts(Info6.m_aSkin, std::size(Info6.m_aSkin), "default");
		Info6.m_UseCustomColor = 0;
		Info6.m_ColorBody = 0;
		Info6.m_ColorFeet = 0;
		pBuilder->NewItem(NETOBJTYPE_CLIENTINFO, i, Info6.AsSlice());
	}

	bool NewGameData = false;

	for(int i = 0; i < pSnapSrcSeven->NumItems(); i++)
	{
		const CSnapshotItem *pItem7 = pSnapSrcSeven->GetItem(i);
		const int ItemType = pSnapSrcSeven->GetItemType(i);
		const int Size = pSnapSrcSeven->GetItemSize(i);
		// the first few items are a full match
		// no translation needed
		if(ItemType == protocol7::NETOBJTYPE_PROJECTILE ||
			ItemType == protocol7::NETOBJTYPE_LASER ||
			ItemType == protocol7::NETOBJTYPE_FLAG)
		{
			pBuilder->NewItem(ItemType, pItem7->Id(), rust::Slice((const int32_t *)pItem7->Data(), Size / sizeof(int32_t)));
		}
		else if(ItemType == protocol7::NETOBJTYPE_PICKUP)
		{
			const protocol7::CNetObj_Pickup *pPickup7 = (const protocol7::CNetObj_Pickup *)pItem7->Data();
			CNetObj_Pickup Pickup6 = {};
			Pickup6.m_X = pPickup7->m_X;
			Pickup6.m_Y = pPickup7->m_Y;
			PickupType_SevenToSix(pPickup7->m_Type, Pickup6.m_Type, Pickup6.m_Subtype);
			pBuilder->NewItem(NETOBJTYPE_PICKUP, pItem7->Id(), Pickup6.AsSlice());
		}
		else if(ItemType == protocol7::NETOBJTYPE_GAMEDATA)
		{
			const protocol7::CNetObj_GameData *pGameData = (const protocol7::CNetObj_GameData *)pItem7->Data();
			TranslationContext.m_GameStateFlags7 = pGameData->m_GameStateFlags;
			TranslationContext.m_GameStartTick7 = pGameData->m_GameStartTick;
			TranslationContext.m_GameStateEndTick7 = pGameData->m_GameStateEndTick;
		}
		else if(ItemType == protocol7::NETOBJTYPE_GAMEDATATEAM)
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
		else if(ItemType == protocol7::NETOBJTYPE_GAMEDATAFLAG)
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
		else if(ItemType == protocol7::NETOBJTYPE_CHARACTER)
		{
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
			pBuilder->NewItem(NETOBJTYPE_CHARACTER, pItem7->Id(), Char6.AsSlice());

			if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_ATTACH_PLAYER)
			{
				CNetEvent_SoundWorld Sound = {};
				Sound.m_X = pChar7->m_X;
				Sound.m_Y = pChar7->m_Y;
				Sound.m_SoundId = SOUND_HOOK_ATTACH_PLAYER;
				pBuilder->NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), Sound.AsSlice());
			}

			if(TranslationContext.m_aLocalClientId[Conn] != pItem7->Id())
			{
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_GROUND_JUMP)
				{
					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_PLAYER_JUMP;
					pBuilder->NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), Sound.AsSlice());
				}
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_ATTACH_GROUND)
				{
					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_HOOK_ATTACH_GROUND;
					pBuilder->NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), Sound.AsSlice());
				}
				if(pChar7->m_TriggeredEvents & protocol7::COREEVENTFLAG_HOOK_HIT_NOHOOK)
				{
					CNetEvent_SoundWorld Sound = {};
					Sound.m_X = pChar7->m_X;
					Sound.m_Y = pChar7->m_Y;
					Sound.m_SoundId = SOUND_HOOK_NOATTACH;
					pBuilder->NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), Sound.AsSlice());
				}
			}
		}
		else if(ItemType == protocol7::NETOBJTYPE_PLAYERINFO)
		{
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
			pBuilder->NewItem(NETOBJTYPE_PLAYERINFO, pItem7->Id(), Info6.AsSlice());
		}
		else if(ItemType == protocol7::NETOBJTYPE_SPECTATORINFO)
		{
			const protocol7::CNetObj_SpectatorInfo *pSpec7 = (const protocol7::CNetObj_SpectatorInfo *)pItem7->Data();
			CNetObj_SpectatorInfo Spec6 = {};
			Spec6.m_SpectatorId = pSpec7->m_SpectatorId;
			if(pSpec7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				Spec6.m_SpectatorId = SPEC_FREEVIEW;
			Spec6.m_X = pSpec7->m_X;
			Spec6.m_Y = pSpec7->m_Y;
			pBuilder->NewItem(NETOBJTYPE_SPECTATORINFO, pItem7->Id(), Spec6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_EXPLOSION)
		{
			const protocol7::CNetEvent_Explosion *pExplosion7 = (const protocol7::CNetEvent_Explosion *)pItem7->Data();
			CNetEvent_Explosion Explosion6 = {};
			Explosion6.m_X = pExplosion7->m_X;
			Explosion6.m_Y = pExplosion7->m_Y;
			pBuilder->NewItem(NETEVENTTYPE_EXPLOSION, pItem7->Id(), Explosion6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_SPAWN)
		{
			const protocol7::CNetEvent_Spawn *pSpawn7 = (const protocol7::CNetEvent_Spawn *)pItem7->Data();
			CNetEvent_Spawn Spawn6 = {};
			Spawn6.m_X = pSpawn7->m_X;
			Spawn6.m_Y = pSpawn7->m_Y;
			pBuilder->NewItem(NETEVENTTYPE_SPAWN, pItem7->Id(), Spawn6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_HAMMERHIT)
		{
			const protocol7::CNetEvent_HammerHit *pHammerHit7 = (const protocol7::CNetEvent_HammerHit *)pItem7->Data();
			CNetEvent_HammerHit HammerHit6 = {};
			HammerHit6.m_X = pHammerHit7->m_X;
			HammerHit6.m_Y = pHammerHit7->m_Y;
			pBuilder->NewItem(NETEVENTTYPE_HAMMERHIT, pItem7->Id(), HammerHit6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_DEATH)
		{
			const protocol7::CNetEvent_Death *pDeath7 = (const protocol7::CNetEvent_Death *)pItem7->Data();
			CNetEvent_Death Death6 = {};
			Death6.m_X = pDeath7->m_X;
			Death6.m_Y = pDeath7->m_Y;
			Death6.m_ClientId = pDeath7->m_ClientId;
			pBuilder->NewItem(NETEVENTTYPE_DEATH, pItem7->Id(), Death6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_SOUNDWORLD)
		{
			const protocol7::CNetEvent_SoundWorld *pSoundWorld7 = (const protocol7::CNetEvent_SoundWorld *)pItem7->Data();
			CNetEvent_SoundWorld SoundWorld6 = {};
			SoundWorld6.m_X = pSoundWorld7->m_X;
			SoundWorld6.m_Y = pSoundWorld7->m_Y;
			SoundWorld6.m_SoundId = pSoundWorld7->m_SoundId;
			pBuilder->NewItem(NETEVENTTYPE_SOUNDWORLD, pItem7->Id(), SoundWorld6.AsSlice());
		}
		else if(ItemType == protocol7::NETEVENTTYPE_DAMAGE)
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
				CNetEvent_DamageInd Dmg6 = {};
				Dmg6.m_X = pDmg7->m_X;
				Dmg6.m_Y = pDmg7->m_Y;
				float f = mix(s, e, float(k + 1) / float(Amount + 2));
				Dmg6.m_Angle = (int)(f * 256.0f);
				// pItem7->Id() is reused that is technically wrong
				// but the client implementation does not look at the ids
				// and renders the damage indicators just fine
				pBuilder->NewItem(NETEVENTTYPE_DAMAGEIND, pItem7->Id(), Dmg6.AsSlice());
			}
		}
		else if(ItemType == protocol7::NETOBJTYPE_DE_CLIENTINFO)
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
			IntsToStr(pInfo->m_aName, std::size(pInfo->m_aName), Client.m_aName, std::size(Client.m_aName));
			IntsToStr(pInfo->m_aClan, std::size(pInfo->m_aClan), Client.m_aClan, std::size(Client.m_aClan));
			Client.m_Country = pInfo->m_Country;

			ApplySkin7InfoFromSnapObj(pInfo, ClientId);
		}
		else if(ItemType == protocol7::NETOBJTYPE_DE_GAMEINFO)
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
		CNetObj_GameData GameData = {};
		GameData.m_TeamscoreRed = TranslationContext.m_TeamscoreRed;
		GameData.m_TeamscoreBlue = TranslationContext.m_TeamscoreBlue;
		GameData.m_FlagCarrierRed = TranslationContext.m_FlagCarrierRed;
		GameData.m_FlagCarrierBlue = TranslationContext.m_FlagCarrierBlue;
		pBuilder->NewItem(NETOBJTYPE_GAMEDATA, 0, GameData.AsSlice());
	}

	return pBuilder->FinishIfNoDroppedItems(*pSnapDstSix);
}

int CGameClient::OnDemoRecSnap7(CSnapshot *pFrom, CSnapshotBuffer *pTo, int Conn)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

	{
		const int Num = pFrom->NumItems();
		for(int i = 0; i < Num; i++)
		{
			const int ItemType = pFrom->GetItemType(i);
			if(ItemType <= 0)
			{
				// Don't add extended item type descriptions, they get added
				// implicitly (== 0).
				//
				// Don't add items of unknown item types either (< 0).
				continue;
			}
			const CSnapshotItem *const pItem = pFrom->GetItem(i);
			const size_t ItemDataLen = pFrom->GetItemSize(i) / sizeof(int32_t);
			dbg_assert(pBuilder->NewItem(ItemType, pItem->Id(), rust::Slice(pItem->Data(), ItemDataLen)), "error re-inserting into snapshot");
		}
	}

	// add client info
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_aClients[i].m_Active)
			continue;

		CTranslationContext::CClientData &ClientData = Client()->m_TranslationContext.m_aClients[i];

		protocol7::CNetObj_De_ClientInfo ClientInfoObj;
		ClientInfoObj.m_Local = i == Client()->m_TranslationContext.m_aLocalClientId[Conn];
		ClientInfoObj.m_Team = ClientData.m_Team;
		StrToInts(ClientInfoObj.m_aName, std::size(ClientInfoObj.m_aName), m_aClients[i].m_aName);
		StrToInts(ClientInfoObj.m_aClan, std::size(ClientInfoObj.m_aClan), m_aClients[i].m_aClan);
		ClientInfoObj.m_Country = ClientData.m_Country;

		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			StrToInts(
				ClientInfoObj.m_aaSkinPartNames[Part],
				std::size(ClientInfoObj.m_aaSkinPartNames[Part]),
				m_aClients[i].m_aSixup[Conn].m_aaSkinPartNames[Part]);
			ClientInfoObj.m_aUseCustomColors[Part] = m_aClients[i].m_aSixup[Conn].m_aUseCustomColors[Part];
			ClientInfoObj.m_aSkinPartColors[Part] = m_aClients[i].m_aSixup[Conn].m_aSkinPartColors[Part];
		}

		pBuilder->NewItem(protocol7::NETOBJTYPE_DE_CLIENTINFO, i, ClientInfoObj.AsSlice());
	}

	// add tuning
	if(mem_comp(&CTuningParams::DEFAULT, &m_aTuning[Conn], sizeof(CTuningParams)) != 0)
	{
		protocol7::CNetObj_De_TuneParams TuneParams;
		mem_copy(&TuneParams.m_aTuneParams, &m_aTuning[Conn], sizeof(TuneParams.m_aTuneParams));
		pBuilder->NewItem(protocol7::NETOBJTYPE_DE_TUNEPARAMS, 0, TuneParams.AsSlice());
	}

	// add game info
	protocol7::CNetObj_De_GameInfo GameInfo;
	GameInfo.m_GameFlags = Client()->m_TranslationContext.m_GameFlags;
	GameInfo.m_ScoreLimit = Client()->m_TranslationContext.m_ScoreLimit;
	GameInfo.m_TimeLimit = Client()->m_TranslationContext.m_TimeLimit;
	GameInfo.m_MatchNum = Client()->m_TranslationContext.m_MatchNum;
	GameInfo.m_MatchCurrent = Client()->m_TranslationContext.m_MatchCurrent;
	pBuilder->NewItem(protocol7::NETOBJTYPE_DE_GAMEINFO, 0, GameInfo.AsSlice());

	return pBuilder->FinishIfNoDroppedItems(*pTo);
}
