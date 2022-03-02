#include <game/client/animstate.h>
#include <game/client/gameclient.h>

#include "players_emoticon.h"

void CPlayersEmoticon::RenderEmoticon(
	const CNetObj_Character *pPlayerChar,
	int ClientID)
{
	if(!in_range(ClientID, MAX_CLIENTS - 1))
		return;

	CNetObj_Character Player = *pPlayerChar;

	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);
	float Alpha = (OtherTeam || ClientID < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;

	vec2 Position = m_pClient->m_aClients[ClientID].m_RenderPos;

	if((Player.m_PlayerFlags & PLAYERFLAG_CHATTING) && !m_pClient->m_aClients[ClientID].m_Afk)
	{
		int CurEmoticon = (SPRITE_DOTDOT - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[CurEmoticon]);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_EmoteQuadContainerIndex, CurEmoticon, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClAfkEmote && m_pClient->m_aClients[ClientID].m_Afk && !(Client()->DummyConnected() && ClientID == m_pClient->m_LocalIDs[!g_Config.m_ClDummy]))
	{
		int CurEmoticon = (SPRITE_ZZZ - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[CurEmoticon]);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_EmoteQuadContainerIndex, CurEmoticon, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClShowEmotes && !m_pClient->m_aClients[ClientID].m_EmoticonIgnore && m_pClient->m_aClients[ClientID].m_EmoticonStartTick != -1)
	{
		float SinceStart = (Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_aClients[ClientID].m_EmoticonStartTick) + (Client()->IntraGameTickSincePrev(g_Config.m_ClDummy) - m_pClient->m_aClients[ClientID].m_EmoticonStartFraction);
		float FromEnd = (2 * Client()->GameTickSpeed()) - SinceStart;

		if(0 <= SinceStart && FromEnd > 0)
		{
			float a = 1;

			if(FromEnd < Client()->GameTickSpeed() / 5)
				a = FromEnd / (Client()->GameTickSpeed() / 5.0f);

			float h = 1;
			if(SinceStart < Client()->GameTickSpeed() / 10)
				h = SinceStart / (Client()->GameTickSpeed() / 10.0f);

			float Wiggle = 0;
			if(SinceStart < Client()->GameTickSpeed() / 5)
				Wiggle = SinceStart / (Client()->GameTickSpeed() / 5.0f);

			float WiggleAngle = sinf(5 * Wiggle);

			Graphics()->QuadsSetRotation(pi / 6 * WiggleAngle);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * Alpha);
			// client_datas::emoticon is an offset from the first emoticon
			Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[m_pClient->m_aClients[ClientID].m_Emoticon]);
			Graphics()->RenderQuadContainerAsSprite(m_EmoteQuadContainerIndex, m_pClient->m_aClients[ClientID].m_Emoticon, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f * h) / 64.f);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0);
		}
	}
}

void CPlayersEmoticon::OnRender()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// only render emoticons for active characters
		if(m_pClient->m_Snap.m_aCharacters[i].m_Active)
		{
			RenderEmoticon(&m_pClient->m_Snap.m_aCharacters[i].m_Cur, i);
		}
	}
}

void CPlayersEmoticon::OnInit()
{
	m_EmoteQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	for(int i = 0; i < NUM_EMOTICONS; ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->QuadContainerAddSprite(m_EmoteQuadContainerIndex, 64.f);
	}
	Graphics()->QuadContainerUpload(m_EmoteQuadContainerIndex);
}
