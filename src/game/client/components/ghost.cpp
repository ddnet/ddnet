/* (c) Rajh, Redix and Sushi. */

#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/client/animstate.h>

#include "race.h"
#include "skins.h"
#include "menus.h"
#include "ghost.h"

CGhost::CGhost()
{
	m_CurPos = 0;
	m_Recording = false;
	m_Rendering = false;
	m_StartRenderTick = -1;
}

void CGhost::GetGhostCharacter(CGhostCharacter_NoTick *pGhostChar, const CNetObj_Character *pChar)
{
	pGhostChar->m_X = pChar->m_X;
	pGhostChar->m_Y = pChar->m_Y;
	pGhostChar->m_VelX = pChar->m_VelX;
	pGhostChar->m_VelY = 0;
	pGhostChar->m_Angle = pChar->m_Angle;
	pGhostChar->m_Direction = pChar->m_Direction;
	pGhostChar->m_Weapon = pChar->m_Weapon;
	pGhostChar->m_HookState = pChar->m_HookState;
	pGhostChar->m_HookX = pChar->m_HookX;
	pGhostChar->m_HookY = pChar->m_HookY;
	pGhostChar->m_AttackTick = pChar->m_AttackTick;
}

void CGhost::AddInfos(const CNetObj_Character *pChar)
{
	// Just to be sure it doesnt eat too much memory, the first test should be enough anyway
	if(m_CurGhost.m_lPath.size() > Client()->GameTickSpeed()*60*20)
	{
		dbg_msg("ghost", "20 minutes elapsed. stopping ghost record");
		StopRecord();
		return;
	}

	CGhostCharacter_NoTick GhostChar;
	GetGhostCharacter(&GhostChar, pChar);
	m_CurGhost.m_lPath.add(GhostChar);
	if(GhostRecorder()->IsRecording())
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_CHARACTER_NO_TICK, (const char*)&GhostChar, sizeof(GhostChar));
}

int CGhost::GetSlot() const
{
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		if(m_aActiveGhosts[i].Empty())
			return i;
	return -1;
}

void CGhost::OnRender()
{
	if(!g_Config.m_ClRaceGhost || Client()->State() != IClient::STATE_ONLINE)
		return;

	// Check if the race line is crossed then start the render of the ghost if one
	bool Start = false;

	std::list < int > Indices = m_pClient->Collision()->GetMapIndices(m_pClient->m_PredictedPrevChar.m_Pos, m_pClient->m_LocalCharacterPos);
	if(!Indices.empty())
	{
		for(std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
			if(m_pClient->Collision()->GetTileIndex(*i) == TILE_BEGIN) Start = true;
	}
	else
	{
		Start = m_pClient->Collision()->GetTileIndex(m_pClient->Collision()->GetPureMapIndex(m_pClient->m_LocalCharacterPos)) == TILE_BEGIN;
	}

	if(!m_Recording && Start)
	{
		StartRender();
		StartRecord();
	}

	CNetObj_Character Char = m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_Cur;
	m_pClient->m_PredictedChar.Write(&Char);

	if(m_pClient->m_NewPredictedTick && m_Recording)
		AddInfos(&Char);

	// Play the ghost
	if(!m_Rendering || !g_Config.m_ClRaceShowGhost)
		return;

	m_CurPos = Client()->PredGameTick() - m_StartRenderTick;

	if(m_CurPos < 0)
	{
		StopRender();
		return;
	}

	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
	{
		CGhostItem *pGhost = &m_aActiveGhosts[i];
		if(m_CurPos >= pGhost->m_lPath.size())
			continue;

		int PrevPos = max(0, m_CurPos-1);
		CGhostCharacter_NoTick Player = pGhost->m_lPath[m_CurPos];
		CGhostCharacter_NoTick Prev = pGhost->m_lPath[PrevPos];

		RenderGhostHook(&Player, &Prev);
		RenderGhost(&Player, &Prev, &pGhost->m_RenderInfo);
	}
}

void CGhost::RenderGhost(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev, CTeeRenderInfo *pInfo)
{
	float IntraTick = Client()->PredIntraGameTick();

	float Angle = mix((float)pPrev->m_Angle, (float)pPlayer->m_Angle, IntraTick)/256.0f;
	vec2 Direction = GetDirection((int)(Angle*256.0f));
	vec2 Position = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pPlayer->m_X, pPlayer->m_Y), IntraTick);
	vec2 Vel = mix(vec2(pPrev->m_VelX/256.0f, pPrev->m_VelY/256.0f), vec2(pPlayer->m_VelX/256.0f, pPlayer->m_VelY/256.0f), IntraTick);

	bool Stationary = pPlayer->m_VelX <= 1 && pPlayer->m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(pPlayer->m_X, pPlayer->m_Y+16);
	bool WantOtherDir = (pPlayer->m_Direction == -1 && Vel.x > 0) || (pPlayer->m_Direction == 1 && Vel.x < 0);

	float WalkTime = fmod(absolute(Position.x), 100.0f)/100.0f;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f);
	else if(Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f);
	else if(!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	if (pPlayer->m_Weapon == WEAPON_GRENADE)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle*pi*2+Angle);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		// normal weapons
		int iw = clamp(pPlayer->m_Weapon, 0, NUM_WEAPONS-1);
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[iw].m_pSpriteBody, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);

		vec2 Dir = Direction;
		float Recoil = 0.0f;
		// TODO: is this correct?
		float a = (Client()->PredGameTick()-pPlayer->m_AttackTick+IntraTick)/5.0f;
		if(a < 1)
			Recoil = sinf(a*pi);

		vec2 p = Position + Dir * g_pData->m_Weapons.m_aId[iw].m_Offsetx - Direction*Recoil*10.0f;
		p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
		RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[iw].m_VisualSize);
		Graphics()->QuadsEnd();
	}

	// Render ghost
	RenderTools()->RenderTee(&State, pInfo, 0, Direction, Position, true);
}

void CGhost::RenderGhostHook(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev)
{
	if(pPrev->m_HookState<=0 || pPlayer->m_HookState<=0)
		return;

	float IntraTick = Client()->PredIntraGameTick();

	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pPlayer->m_X, pPlayer->m_Y), IntraTick);

	vec2 HookPos = mix(vec2(pPrev->m_HookX, pPrev->m_HookY), vec2(pPlayer->m_HookX, pPlayer->m_HookY), IntraTick);
	float d = distance(Pos, HookPos);
	vec2 Dir = normalize(Pos-HookPos);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(GetAngle(Dir)+pi);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

	// render head
	RenderTools()->SelectSprite(SPRITE_HOOK_HEAD);
	IGraphics::CQuadItem QuadItem(HookPos.x, HookPos.y, 24, 16);
	Graphics()->QuadsDraw(&QuadItem, 1);

	// render chain
	RenderTools()->SelectSprite(SPRITE_HOOK_CHAIN);
	IGraphics::CQuadItem Array[1024];
	int j = 0;
	for(float f = 24; f < d && j < 1024; f += 24, j++)
	{
		vec2 p = HookPos + Dir*f;
		Array[j] = IGraphics::CQuadItem(p.x, p.y, 24, 16);
	}

	Graphics()->QuadsDraw(Array, j);
	Graphics()->QuadsSetRotation(0);
	Graphics()->QuadsEnd();
}

void CGhost::InitRenderInfos(CTeeRenderInfo *pRenderInfo, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	int SkinId = m_pClient->m_pSkins->Find(pSkinName);
	if(SkinId < 0)
	{
		SkinId = m_pClient->m_pSkins->Find("default");
		if(SkinId < 0)
			SkinId = 0;
	}

	pRenderInfo->m_ColorBody = m_pClient->m_pSkins->GetColorV4(ColorBody);
	pRenderInfo->m_ColorFeet = m_pClient->m_pSkins->GetColorV4(ColorFeet);

	if(UseCustomColor)
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_ColorTexture;
	else
	{
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_OrgTexture;
		pRenderInfo->m_ColorBody = vec4(1, 1, 1, 1);
		pRenderInfo->m_ColorFeet = vec4(1, 1, 1, 1);
	}

	pRenderInfo->m_ColorBody.a = 0.5f;
	pRenderInfo->m_ColorFeet.a = 0.5f;
	pRenderInfo->m_Size = 64;
}

void CGhost::StartRecord()
{
	m_Recording = true;
	m_CurGhost.Reset();
	
	CGameClient::CClientData ClientData = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID];
	InitRenderInfos(&m_CurGhost.m_RenderInfo, ClientData.m_aSkinName, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);

	if(g_Config.m_ClRaceSaveGhost)
	{
		Client()->GhostRecorder_Start();

		CGhostSkin Skin;
		StrToInts(&Skin.m_Skin0, 6, ClientData.m_aSkinName);
		Skin.m_UseCustomColor = ClientData.m_UseCustomColor;
		Skin.m_ColorBody = ClientData.m_ColorBody;
		Skin.m_ColorFeet = ClientData.m_ColorFeet;
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&Skin, sizeof(Skin));
	}
}

void CGhost::StopRecord(int Time)
{
	m_Recording = false;
	bool RecordingToFile = GhostRecorder()->IsRecording();

	if(RecordingToFile)
		GhostRecorder()->Stop(m_CurGhost.m_lPath.size(), Time);

	char aTmpFilename[128];
	Client()->Ghost_GetPath(aTmpFilename, sizeof(aTmpFilename));

	CMenus::CGhostItem *pOwnGhost = m_pClient->m_pMenus->GetOwnGhost();
	if(Time > 0 && (!pOwnGhost || Time < pOwnGhost->m_Time))
	{
		// add to active ghosts
		int Slot = pOwnGhost ? pOwnGhost->m_Slot : GetSlot();
		if(Slot != -1)
			m_aActiveGhosts[Slot] = m_CurGhost;

		char aFilename[128] = { 0 };
		if(RecordingToFile)
		{
			// remove old ghost
			if(pOwnGhost && pOwnGhost->HasFile())
				Storage()->RemoveFile(pOwnGhost->m_aFilename, IStorage::TYPE_SAVE);

			// save new ghost
			Client()->Ghost_GetPath(aFilename, sizeof(aFilename), Time);
			Storage()->RenameFile(aTmpFilename, aFilename, IStorage::TYPE_SAVE);
		}

		// update menu list item
		if(!pOwnGhost)
		{
			int Index = m_pClient->m_pMenus->m_lGhosts.add(CMenus::CGhostItem());
			pOwnGhost = &m_pClient->m_pMenus->m_lGhosts[Index];
		}
		
		str_copy(pOwnGhost->m_aFilename, aFilename, sizeof(pOwnGhost->m_aFilename));
		str_copy(pOwnGhost->m_aPlayer, g_Config.m_PlayerName, sizeof(pOwnGhost->m_aPlayer));
		pOwnGhost->m_Time = Time;
		pOwnGhost->m_Slot = Slot;
		pOwnGhost->m_Own = true;

		m_pClient->m_pMenus->m_lGhosts.sort_range();
	}
	else if(RecordingToFile) // no new record
		Storage()->RemoveFile(aTmpFilename, IStorage::TYPE_SAVE);

	m_CurGhost.Reset();
}

void CGhost::StartRender()
{
	m_CurPos = 0;
	m_Rendering = true;
	m_StartRenderTick = Client()->PredGameTick();
}

void CGhost::StopRender()
{
	m_Rendering = false;
}

int CGhost::Load(const char *pFilename)
{
	int Slot = GetSlot();
	if(Slot == -1)
		return -1;

	if(!Client()->GhostLoader_Load(pFilename))
		return -1;

	const CGhostHeader *pHeader = GhostLoader()->GetHeader();

	int NumTicks = GhostLoader()->GetTicks(pHeader);
	int Time = GhostLoader()->GetTime(pHeader);
	if(NumTicks <= 0 || Time <= 0)
	{
		GhostLoader()->Close();
		return -1;
	}

	// select ghost
	CGhostItem *pGhost = &m_aActiveGhosts[Slot];
	pGhost->m_lPath.set_size(NumTicks);

	int Index = 0;
	bool FoundSkin = false;

	int Type;
	while(GhostLoader()->ReadNextType(&Type))
	{
		if(Index == NumTicks && Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK)
		{
			Index = -1;
			break;
		}

		if(Type == GHOSTDATA_TYPE_SKIN)
		{
			CGhostSkin Skin;
			if(GhostLoader()->ReadData(Type, (char*)&Skin, sizeof(Skin)) && !FoundSkin)
			{
				FoundSkin = true;
				char aSkinName[64];
				IntsToStr(&Skin.m_Skin0, 6, aSkinName);
				InitRenderInfos(&pGhost->m_RenderInfo, aSkinName, Skin.m_UseCustomColor, Skin.m_ColorBody, Skin.m_ColorFeet);
			}
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK)
		{
			CGhostCharacter_NoTick Char;
			GhostLoader()->ReadData(Type, (char*)&pGhost->m_lPath[Index++], sizeof(Char));
		}
	}

	GhostLoader()->Close();

	if(Index != NumTicks)
	{
		pGhost->Reset();
		return -1;
	}

	if(!FoundSkin)
		InitRenderInfos(&pGhost->m_RenderInfo, "default", 0, 0, 0);

	return Slot;
}

void CGhost::Unload(int Slot)
{
	m_aActiveGhosts[Slot].Reset();
}

void CGhost::ConGPlay(IConsole::IResult *pResult, void *pUserData)
{
	((CGhost *)pUserData)->StartRender();
}

void CGhost::OnConsoleInit()
{
	m_pGhostLoader = Kernel()->RequestInterface<IGhostLoader>();
	m_pGhostRecorder = Kernel()->RequestInterface<IGhostRecorder>();

	Console()->Register("gplay", "", CFGFLAG_CLIENT, ConGPlay, this, "");
}

void CGhost::OnMessage(int MsgType, void *pRawMsg)
{
	if(!g_Config.m_ClRaceGhost || Client()->State() != IClient::STATE_ONLINE || m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;

	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
			OnReset();
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID == -1 && m_Recording)
		{
			char aName[MAX_NAME_LENGTH];
			int Time = CRaceHelper::TimeFromFinishMessage(pMsg->m_pMessage, aName, sizeof(aName));
			if(Time > 0 && str_comp(aName, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName) == 0)
			{
				StopRecord(Time);
				StopRender();
			}
		}
	}
}

void CGhost::OnReset()
{
	StopRecord();
	StopRender();
}

void CGhost::OnMapLoad()
{
	OnReset();
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		Unload(i);
	m_pClient->m_pMenus->GhostlistPopulate();
}
