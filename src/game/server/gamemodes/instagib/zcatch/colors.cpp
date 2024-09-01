#include <game/server/player.h>
#include <game/server/teeinfo.h>

#include "zcatch.h"

static int ColorToSixup(int Color6)
{
	return ColorHSLA(Color6).UnclampLighting().Pack(CTeeInfo::ms_DarkestLGT7);
}

int CGameControllerZcatch::GetBodyColorTeetime(int Kills)
{
	return maximum(0, 160 - Kills * 10) * 0x010000 + 0xff00;
}

int CGameControllerZcatch::GetBodyColorSavander(int Kills)
{
	if(Kills == 0)
		return 0xFFBB00;
	if(Kills == 1)
		return 0x00FF00;
	if(Kills == 2)
		return 0x11FF00;
	if(Kills == 3)
		return 0x22FF00;
	if(Kills == 4)
		return 0x33FF00;
	if(Kills == 5)
		return 0x44FF00;
	if(Kills == 6)
		return 0x55FF00;
	if(Kills == 7)
		return 0x66FF00;
	if(Kills == 8)
		return 0x77FF00;
	if(Kills == 9)
		return 0x88FF00;
	if(Kills == 10)
		return 0x99FF00;
	if(Kills == 11)
		return 0xAAFF00;
	if(Kills == 12)
		return 0xBBFF00;
	if(Kills == 13)
		return 0xCCFF00;
	if(Kills == 14)
		return 0xDDFF00;
	if(Kills == 15)
		return 0xEEFF00;
	return 0xFFBB00;
}

int CGameControllerZcatch::GetBodyColor(int Kills)
{
	switch(m_CatchColors)
	{
	case ECatchColors::TEETIME:
		return GetBodyColorTeetime(Kills);
	case ECatchColors::SAVANDER:
		return GetBodyColorSavander(Kills);
	default:
		dbg_assert(true, "invalid color");
	}
	return 0;
}

void CGameControllerZcatch::OnUpdateZcatchColorConfig()
{
	const char *pColor = Config()->m_SvZcatchColors;

	if(!str_comp_nocase(pColor, "teetime"))
		m_CatchColors = ECatchColors::TEETIME;
	else if(!str_comp_nocase(pColor, "savander"))
		m_CatchColors = ECatchColors::SAVANDER;
	else
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Error: invalid zcatch color scheme '%s' defaulting to 'teetime'", pColor);
		Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"ddnet-insta",
			aBuf);
		str_copy(Config()->m_SvZcatchColors, "teetime");
	}
}

void CGameControllerZcatch::SetCatchColors(CPlayer *pPlayer)
{
	int Color = GetBodyColor(pPlayer->m_Spree);

	// it would be cleaner if this only applied to the winner
	// we could make sure m_Spree is not reset until the next round starts
	// but for now it should work because players that connect during round end
	// will reset m_aBodyColors
	if(GameState() == IGS_END_ROUND)
		Color = m_aBodyColors[pPlayer->GetCid()];

	// 0.6
	pPlayer->m_TeeInfos.m_ColorBody = Color;
	pPlayer->m_TeeInfos.m_UseCustomColor = 1;

	// 0.7
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		pPlayer->m_TeeInfos.m_aSkinPartColors[p] = ColorToSixup(Color);
		pPlayer->m_TeeInfos.m_aUseCustomColors[p] = true;
	}
}

bool CGameControllerZcatch::OnChangeInfoNetMessage(const CNetMsg_Cl_ChangeInfo *pMsg, int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	SetCatchColors(pPlayer);
	return false;
}

void CGameControllerZcatch::SendSkinBodyColor7(int ClientId, int Color)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	// also update 0.6 just to be sure
	pPlayer->m_TeeInfos.m_ColorBody = Color;
	pPlayer->m_TeeInfos.m_UseCustomColor = 1;

	// 0.7
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		pPlayer->m_TeeInfos.m_aSkinPartColors[p] = ColorToSixup(Color);
		pPlayer->m_TeeInfos.m_aUseCustomColors[p] = true;
	}

	protocol7::CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientId = ClientId;
	for(int p = 0; p < protocol7::NUM_SKINPARTS; p++)
	{
		Msg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
		Msg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
		Msg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
	}

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
}
