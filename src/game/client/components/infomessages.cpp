/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/localization.h>

#include "infomessages.h"
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

static constexpr float ROW_HEIGHT = 46.0f;
static constexpr float FONT_SIZE = 36.0f;
static constexpr float RACE_FLAG_SIZE = 52.0f;

void CInfoMessages::OnWindowResize()
{
	for(auto &InfoMsg : m_aInfoMsgs)
	{
		DeleteTextContainers(InfoMsg);
	}
}

void CInfoMessages::OnReset()
{
	m_InfoMsgCurrent = 0;
	for(auto &InfoMsg : m_aInfoMsgs)
	{
		ResetMessage(InfoMsg);
	}
}

void CInfoMessages::DeleteTextContainers(CInfoMsg &InfoMsg)
{
	TextRender()->DeleteTextContainer(InfoMsg.m_VictimTextContainerIndex);
	TextRender()->DeleteTextContainer(InfoMsg.m_KillerTextContainerIndex);
	TextRender()->DeleteTextContainer(InfoMsg.m_DiffTextContainerIndex);
	TextRender()->DeleteTextContainer(InfoMsg.m_TimeTextContainerIndex);
}

void CInfoMessages::ResetMessage(CInfoMsg &InfoMsg)
{
	InfoMsg.m_Tick = -1;
	std::fill(std::begin(InfoMsg.m_apVictimManagedTeeRenderInfos), std::end(InfoMsg.m_apVictimManagedTeeRenderInfos), nullptr);
	InfoMsg.m_pKillerManagedTeeRenderInfo = nullptr;
	DeleteTextContainers(InfoMsg);
}

void CInfoMessages::OnInit()
{
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	m_SpriteQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	Graphics()->QuadsSetSubset(1, 0, 0, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	Graphics()->QuadsSetSubset(1, 0, 0, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 96.f * ScaleX, 96.f * ScaleY);
	}

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_QuadOffsetRaceFlag = RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.0f, 0.0f, RACE_FLAG_SIZE, RACE_FLAG_SIZE);

	Graphics()->QuadContainerUpload(m_SpriteQuadContainerIndex);
}

CInfoMessages::CInfoMsg CInfoMessages::CreateInfoMsg(EType Type)
{
	CInfoMsg InfoMsg;
	InfoMsg.m_Type = Type;
	InfoMsg.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

	for(int i = 0; i < MAX_KILLMSG_TEAM_MEMBERS; i++)
	{
		InfoMsg.m_aVictimIds[i] = -1;
		InfoMsg.m_apVictimManagedTeeRenderInfos[i] = nullptr;
	}
	InfoMsg.m_VictimDDTeam = 0;
	InfoMsg.m_aVictimName[0] = '\0';
	InfoMsg.m_VictimTextContainerIndex.Reset();

	InfoMsg.m_KillerId = -1;
	InfoMsg.m_aKillerName[0] = '\0';
	InfoMsg.m_KillerTextContainerIndex.Reset();
	InfoMsg.m_pKillerManagedTeeRenderInfo = nullptr;

	InfoMsg.m_Weapon = -1;
	InfoMsg.m_ModeSpecial = 0;
	InfoMsg.m_FlagCarrierBlue = -1;
	InfoMsg.m_TeamSize = 0;

	InfoMsg.m_Diff = 0;
	InfoMsg.m_aTimeText[0] = '\0';
	InfoMsg.m_aDiffText[0] = '\0';
	InfoMsg.m_TimeTextContainerIndex.Reset();
	InfoMsg.m_DiffTextContainerIndex.Reset();
	InfoMsg.m_RecordPersonal = false;
	return InfoMsg;
}

void CInfoMessages::AddInfoMsg(const CInfoMsg &InfoMsg)
{
	dbg_assert(InfoMsg.m_TeamSize >= 0 && InfoMsg.m_TeamSize <= MAX_KILLMSG_TEAM_MEMBERS, "Info message team size invalid");
	dbg_assert(InfoMsg.m_KillerId < 0 || InfoMsg.m_pKillerManagedTeeRenderInfo != nullptr, "Info message killer invalid");
	for(int i = 0; i < InfoMsg.m_TeamSize; i++)
	{
		dbg_assert(InfoMsg.m_aVictimIds[i] >= 0 && InfoMsg.m_apVictimManagedTeeRenderInfos[i] != nullptr, "Info message victim invalid");
	}

	const float Height = 1.5f * 400.0f * 3.0f;
	const float Width = Height * Graphics()->ScreenAspect();

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->MapScreen(0, 0, Width, Height);

	m_InfoMsgCurrent = (m_InfoMsgCurrent + 1) % MAX_INFOMSGS;
	DeleteTextContainers(m_aInfoMsgs[m_InfoMsgCurrent]);
	m_aInfoMsgs[m_InfoMsgCurrent] = InfoMsg;
	CreateTextContainersIfNotCreated(m_aInfoMsgs[m_InfoMsgCurrent]);

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CInfoMessages::CreateTextContainersIfNotCreated(CInfoMsg &InfoMsg)
{
	const auto &&NameColor = [&](int ClientId) -> ColorRGBA {
		unsigned Color;
		if(ClientId == GameClient()->m_Snap.m_LocalClientId)
		{
			Color = g_Config.m_ClKillMessageHighlightColor;
		}
		else
		{
			Color = g_Config.m_ClKillMessageNormalColor;
		}
		return color_cast<ColorRGBA>(ColorHSLA(Color));
	};

	if(!InfoMsg.m_VictimTextContainerIndex.Valid() && InfoMsg.m_aVictimName[0] != '\0')
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FONT_SIZE, TEXTFLAG_RENDER);
		TextRender()->TextColor(NameColor(InfoMsg.m_aVictimIds[0]));
		TextRender()->CreateTextContainer(InfoMsg.m_VictimTextContainerIndex, &Cursor, InfoMsg.m_aVictimName);
	}

	if(!InfoMsg.m_KillerTextContainerIndex.Valid() && InfoMsg.m_aKillerName[0] != '\0')
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FONT_SIZE, TEXTFLAG_RENDER);
		TextRender()->TextColor(NameColor(InfoMsg.m_KillerId));
		TextRender()->CreateTextContainer(InfoMsg.m_KillerTextContainerIndex, &Cursor, InfoMsg.m_aKillerName);
	}

	if(!InfoMsg.m_DiffTextContainerIndex.Valid() && InfoMsg.m_aDiffText[0] != '\0')
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FONT_SIZE, TEXTFLAG_RENDER);

		if(InfoMsg.m_Diff > 0)
			TextRender()->TextColor(1.0f, 0.5f, 0.5f, 1.0f); // red
		else if(InfoMsg.m_Diff < 0)
			TextRender()->TextColor(0.5f, 1.0f, 0.5f, 1.0f); // green
		else
			TextRender()->TextColor(TextRender()->DefaultTextColor());

		TextRender()->CreateTextContainer(InfoMsg.m_DiffTextContainerIndex, &Cursor, InfoMsg.m_aDiffText);
	}

	if(!InfoMsg.m_TimeTextContainerIndex.Valid() && InfoMsg.m_aTimeText[0] != '\0')
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FONT_SIZE, TEXTFLAG_RENDER);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->CreateTextContainer(InfoMsg.m_TimeTextContainerIndex, &Cursor, InfoMsg.m_aTimeText);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CInfoMessages::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	switch(MsgType)
	{
	case NETMSGTYPE_SV_KILLMSGTEAM:
		OnTeamKillMessage(static_cast<CNetMsg_Sv_KillMsgTeam *>(pRawMsg));
		break;
	case NETMSGTYPE_SV_KILLMSG:
		OnKillMessage(static_cast<CNetMsg_Sv_KillMsg *>(pRawMsg));
		break;
	case NETMSGTYPE_SV_RACEFINISH:
		OnRaceFinishMessage(static_cast<CNetMsg_Sv_RaceFinish *>(pRawMsg));
		break;
	}
}

void CInfoMessages::OnTeamKillMessage(const CNetMsg_Sv_KillMsgTeam *pMsg)
{
	std::vector<std::pair<int, int>> vStrongWeakSorted;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(GameClient()->m_aClients[ClientId].m_Active &&
			GameClient()->m_Teams.Team(ClientId) == pMsg->m_Team)
		{
			CCharacter *pChr = GameClient()->m_GameWorld.GetCharacterById(ClientId);
			vStrongWeakSorted.emplace_back(ClientId, pMsg->m_First == ClientId ? MAX_CLIENTS : pChr ? pChr->GetStrongWeakId() : 0);
		}
	}
	std::stable_sort(vStrongWeakSorted.begin(), vStrongWeakSorted.end(), [](auto &Left, auto &Right) { return Left.second > Right.second; });

	CInfoMsg Kill = CreateInfoMsg(TYPE_KILL);
	Kill.m_TeamSize = minimum<int>(vStrongWeakSorted.size(), MAX_KILLMSG_TEAM_MEMBERS);

	Kill.m_VictimDDTeam = pMsg->m_Team;
	for(int i = 0; i < Kill.m_TeamSize; i++)
	{
		const int VictimId = vStrongWeakSorted[i].first;
		Kill.m_aVictimIds[i] = VictimId;
		Kill.m_apVictimManagedTeeRenderInfos[i] = GameClient()->CreateManagedTeeRenderInfo(GameClient()->m_aClients[VictimId]);
	}
	str_format(Kill.m_aVictimName, sizeof(Kill.m_aVictimName), Localize("Team %d"), pMsg->m_Team);

	AddInfoMsg(Kill);
}

void CInfoMessages::OnKillMessage(const CNetMsg_Sv_KillMsg *pMsg)
{
	CInfoMsg Kill = CreateInfoMsg(TYPE_KILL);

	if(GameClient()->m_aClients[pMsg->m_Victim].m_Active)
	{
		Kill.m_TeamSize = 1;
		Kill.m_aVictimIds[0] = pMsg->m_Victim;
		Kill.m_VictimDDTeam = GameClient()->m_Teams.Team(Kill.m_aVictimIds[0]);
		str_copy(Kill.m_aVictimName, GameClient()->m_aClients[Kill.m_aVictimIds[0]].m_aName);
		Kill.m_apVictimManagedTeeRenderInfos[0] = GameClient()->CreateManagedTeeRenderInfo(GameClient()->m_aClients[Kill.m_aVictimIds[0]]);
	}

	if(GameClient()->m_aClients[pMsg->m_Killer].m_Active)
	{
		Kill.m_KillerId = pMsg->m_Killer;
		str_copy(Kill.m_aKillerName, GameClient()->m_aClients[Kill.m_KillerId].m_aName);
		Kill.m_pKillerManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(GameClient()->m_aClients[Kill.m_KillerId]);
	}

	Kill.m_Weapon = pMsg->m_Weapon;
	Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
	Kill.m_FlagCarrierBlue = GameClient()->m_Snap.m_pGameDataObj ? GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : -1;

	if(Kill.m_TeamSize == 0 && Kill.m_KillerId == -1 && Kill.m_Weapon < 0)
	{
		return; // message would be empty
	}

	AddInfoMsg(Kill);
}

void CInfoMessages::OnRaceFinishMessage(const CNetMsg_Sv_RaceFinish *pMsg)
{
	if(!GameClient()->m_aClients[pMsg->m_ClientId].m_Active)
	{
		return;
	}

	CInfoMsg Finish = CreateInfoMsg(TYPE_FINISH);

	Finish.m_TeamSize = 1;
	Finish.m_aVictimIds[0] = pMsg->m_ClientId;
	Finish.m_VictimDDTeam = GameClient()->m_Teams.Team(Finish.m_aVictimIds[0]);
	str_copy(Finish.m_aVictimName, GameClient()->m_aClients[Finish.m_aVictimIds[0]].m_aName);
	Finish.m_apVictimManagedTeeRenderInfos[0] = GameClient()->CreateManagedTeeRenderInfo(GameClient()->m_aClients[pMsg->m_ClientId]);

	Finish.m_Diff = pMsg->m_Diff;
	Finish.m_RecordPersonal = pMsg->m_RecordPersonal || pMsg->m_RecordServer;
	if(Finish.m_Diff)
	{
		char aBuf[64];
		str_time_float(absolute(Finish.m_Diff) / 1000.0f, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
		str_format(Finish.m_aDiffText, sizeof(Finish.m_aDiffText), "(%c%s)", Finish.m_Diff < 0 ? '-' : '+', aBuf);
	}
	str_time_float(pMsg->m_Time / 1000.0f, TIME_HOURS_CENTISECS, Finish.m_aTimeText, sizeof(Finish.m_aTimeText));

	AddInfoMsg(Finish);
}

void CInfoMessages::RenderKillMsg(const CInfoMsg &InfoMsg, float x, float y)
{
	ColorRGBA TextColor;
	if(InfoMsg.m_VictimDDTeam)
		TextColor = GameClient()->GetDDTeamColor(InfoMsg.m_VictimDDTeam, 0.75f);
	else
		TextColor = TextRender()->DefaultTextColor();

	// render victim name
	if(InfoMsg.m_VictimTextContainerIndex.Valid())
	{
		x -= TextRender()->GetBoundingBoxTextContainer(InfoMsg.m_VictimTextContainerIndex).m_W;
		TextRender()->RenderTextContainer(InfoMsg.m_VictimTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (ROW_HEIGHT - FONT_SIZE) / 2.0f);
	}

	// render victim flag
	x -= 24.0f;
	if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) && (InfoMsg.m_ModeSpecial & 1))
	{
		int QuadOffset;
		if(InfoMsg.m_aVictimIds[0] == InfoMsg.m_FlagCarrierBlue)
		{
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
			QuadOffset = 0;
		}
		else
		{
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
			QuadOffset = 1;
		}
		Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x, y - 16);
	}

	// render victim tees
	for(int j = (InfoMsg.m_TeamSize - 1); j >= 0; j--)
	{
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &InfoMsg.m_apVictimManagedTeeRenderInfos[j]->TeeRenderInfo(), OffsetToMid);
		const vec2 TeeRenderPos = vec2(x, y + ROW_HEIGHT / 2.0f + OffsetToMid.y);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &InfoMsg.m_apVictimManagedTeeRenderInfos[j]->TeeRenderInfo(), EMOTE_PAIN, vec2(-1, 0), TeeRenderPos);
		x -= 44.0f;
	}

	// render weapon
	x -= 32.0f;
	if(InfoMsg.m_Weapon >= 0)
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeapons[InfoMsg.m_Weapon]);
		Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, 4 + InfoMsg.m_Weapon, x, y + 28);
	}
	x -= 52.0f;

	// render killer (only if different from victim)
	if(InfoMsg.m_aVictimIds[0] != InfoMsg.m_KillerId)
	{
		// render killer flag
		if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) && (InfoMsg.m_ModeSpecial & 2))
		{
			int QuadOffset;
			if(InfoMsg.m_KillerId == InfoMsg.m_FlagCarrierBlue)
			{
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
				QuadOffset = 2;
			}
			else
			{
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
				QuadOffset = 3;
			}
			Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x - 56, y - 16);
		}

		// render killer tee
		x -= 24.0f;
		if(InfoMsg.m_pKillerManagedTeeRenderInfo != nullptr)
		{
			vec2 OffsetToMid;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &InfoMsg.m_pKillerManagedTeeRenderInfo->TeeRenderInfo(), OffsetToMid);
			const vec2 TeeRenderPos = vec2(x, y + ROW_HEIGHT / 2.0f + OffsetToMid.y);
			RenderTools()->RenderTee(CAnimState::GetIdle(), &InfoMsg.m_pKillerManagedTeeRenderInfo->TeeRenderInfo(), EMOTE_ANGRY, vec2(1, 0), TeeRenderPos);
		}
		x -= 32.0f;

		// render killer name
		if(InfoMsg.m_KillerTextContainerIndex.Valid())
		{
			x -= TextRender()->GetBoundingBoxTextContainer(InfoMsg.m_KillerTextContainerIndex).m_W;
			TextRender()->RenderTextContainer(InfoMsg.m_KillerTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (ROW_HEIGHT - FONT_SIZE) / 2.0f);
		}
	}
}

void CInfoMessages::RenderFinishMsg(const CInfoMsg &InfoMsg, float x, float y)
{
	// render time diff
	if(InfoMsg.m_DiffTextContainerIndex.Valid())
	{
		x -= TextRender()->GetBoundingBoxTextContainer(InfoMsg.m_DiffTextContainerIndex).m_W;
		TextRender()->RenderTextContainer(InfoMsg.m_DiffTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), x, y + (ROW_HEIGHT - FONT_SIZE) / 2.0f);
	}

	// render time
	if(InfoMsg.m_TimeTextContainerIndex.Valid())
	{
		x -= TextRender()->GetBoundingBoxTextContainer(InfoMsg.m_TimeTextContainerIndex).m_W;
		TextRender()->RenderTextContainer(InfoMsg.m_TimeTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), x, y + (ROW_HEIGHT - FONT_SIZE) / 2.0f);
	}

	// render flag
	x -= RACE_FLAG_SIZE;
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_RACEFLAG].m_Id);
	Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, m_QuadOffsetRaceFlag, x, y);

	// render victim name
	if(InfoMsg.m_VictimTextContainerIndex.Valid())
	{
		x -= TextRender()->GetBoundingBoxTextContainer(InfoMsg.m_VictimTextContainerIndex).m_W;
		ColorRGBA TextColor;
		if(InfoMsg.m_VictimDDTeam)
			TextColor = GameClient()->GetDDTeamColor(InfoMsg.m_VictimDDTeam, 0.75f);
		else
			TextColor = TextRender()->DefaultTextColor();
		TextRender()->RenderTextContainer(InfoMsg.m_VictimTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (ROW_HEIGHT - FONT_SIZE) / 2.0f);
	}

	// render victim tee
	x -= 24.0f;
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &InfoMsg.m_apVictimManagedTeeRenderInfos[0]->TeeRenderInfo(), OffsetToMid);
	const vec2 TeeRenderPos = vec2(x, y + ROW_HEIGHT / 2.0f + OffsetToMid.y);
	const int Emote = InfoMsg.m_RecordPersonal ? EMOTE_HAPPY : EMOTE_NORMAL;
	RenderTools()->RenderTee(CAnimState::GetIdle(), &InfoMsg.m_apVictimManagedTeeRenderInfos[0]->TeeRenderInfo(), Emote, vec2(-1, 0), TeeRenderPos);
}

void CInfoMessages::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Height = 1.5f * 400.0f * 3.0f;
	const float Width = Height * Graphics()->ScreenAspect();

	Graphics()->MapScreen(0, 0, Width, Height);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	int Showfps = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		Showfps = 0;
#endif
	const float StartX = Width - 10.0f;
	const float StartY = 30.0f + (Showfps ? 100.0f : 0.0f) + (g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK ? 100.0f : 0.0f);

	float y = StartY;
	for(int i = 1; i <= MAX_INFOMSGS; i++)
	{
		CInfoMsg &InfoMsg = m_aInfoMsgs[(m_InfoMsgCurrent + i) % MAX_INFOMSGS];
		if(InfoMsg.m_Tick == -1)
		{
			continue;
		}
		if(Client()->GameTick(g_Config.m_ClDummy) > InfoMsg.m_Tick + Client()->GameTickSpeed() * 10)
		{
			ResetMessage(InfoMsg);
			continue;
		}

		CreateTextContainersIfNotCreated(InfoMsg);

		if(InfoMsg.m_Type == EType::TYPE_KILL && g_Config.m_ClShowKillMessages)
		{
			RenderKillMsg(InfoMsg, StartX, y);
			y += ROW_HEIGHT;
		}
		else if(InfoMsg.m_Type == EType::TYPE_FINISH && g_Config.m_ClShowFinishMessages)
		{
			RenderFinishMsg(InfoMsg, StartX, y);
			y += ROW_HEIGHT;
		}
	}
}
