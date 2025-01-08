#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include <game/client/render.h>
#include <game/client/ui.h>

#include "statusbar.h"
#include <game/client/gameclient.h>

int CStatusBar::GetDigitsIndex(const int Value, const int Max)
{
	int s_Value = Value;
	if(s_Value < 0) // Normalize
		s_Value *= -1;

	int DigitsIndex = static_cast<int>(log10((s_Value ? s_Value : 1)));

	if(DigitsIndex > Max)
		DigitsIndex = Max;
	if(DigitsIndex < 0)
		DigitsIndex = 0;

	return DigitsIndex;
}
float CStatusBar::GetDurationWidth(int Duration)
{
	static float s_FontSize = 0.0f;
	static float s_TextWidthM = 0.0f, s_TextWidthH = 0.0f, s_TextWidth0D = 0.0f, s_TextWidth00D = 0.0f, s_TextWidth000D = 0.0f;
	if(s_FontSize != m_FontSize)
	{
		s_TextWidthM = TextRender()->TextWidth(m_FontSize, "00:00");
		s_TextWidthH = TextRender()->TextWidth(m_FontSize, "00:00:00");
		s_TextWidth0D = TextRender()->TextWidth(m_FontSize, "0d 00:00:00");
		s_TextWidth00D = TextRender()->TextWidth(m_FontSize, "00d 00:00:00");
		s_TextWidth000D = TextRender()->TextWidth(m_FontSize, "000d 00:00:00");
		s_FontSize = m_FontSize;
	}
	return Duration >= 3600 * 24 * 100 ? s_TextWidth000D : Duration >= 3600 * 24 * 10 ? s_TextWidth00D : Duration >= 3600 * 24 ? s_TextWidth0D : Duration >= 3600 ? s_TextWidthH : s_TextWidthM;
}

float CStatusBar::AngleWidth()
{
	if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
		return 0.0f;

	return TextRender()->TextWidth(m_FontSize, "000.00");
}
void CStatusBar::AngleRender()
{
	CNetObj_Character *pCharacter = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_Cur;
	float Angle = 0.0f;
	if(m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_HasExtendedDisplayInfo)
	{
		CNetObj_DDNetCharacter *pExtendedData = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_ExtendedData;
		Angle = atan2f(pExtendedData->m_TargetY, pExtendedData->m_TargetX);
	}
	else
		Angle = pCharacter->m_Angle / 256.0f;
	if(Angle < 0)
		Angle += 2.0f * pi;
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.2f", Angle * 180.0f / pi);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::PingWidth()
{
	if(!m_pClient->m_Snap.m_apPlayerInfos[m_PlayerId])
		return 0.0f;

	return TextRender()->TextWidth(m_FontSize, "0000");
}
void CStatusBar::PingRender()
{
	const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apPlayerInfos[m_PlayerId];
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%d", pInfo->m_Latency);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
	m_PingActive = true;
}

float CStatusBar::PredictionWidth()
{
	return TextRender()->TextWidth(m_FontSize, "0000");
}
void CStatusBar::PredictionRender()
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%d", Client()->GetPredictionTime());
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::LocalTimeWidth()
{
	return TextRender()->TextWidth(m_FontSize,
		g_Config.m_ClStatusBar12HourClock ? (g_Config.m_ClStatusBarLocalTimeSeocnds ? "00:00:00 XX" : "00:00 XX") : (g_Config.m_ClStatusBarLocalTimeSeocnds ? "00:00:00" : "00:00"));
}
void CStatusBar::LocalTimeRender()
{
	static char aTimeBuf[12] = {0};
	str_timestamp_format(aTimeBuf, sizeof(aTimeBuf), g_Config.m_ClStatusBar12HourClock ? (g_Config.m_ClStatusBarLocalTimeSeocnds ? "%I:%M:%S %p" : "%I:%M %p") : (g_Config.m_ClStatusBarLocalTimeSeocnds ? "%H:%M:%S" : "%H:%M"));
	if(aTimeBuf[0] == '0')
		str_copy(aTimeBuf, &aTimeBuf[1], sizeof(aTimeBuf) - 1);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aTimeBuf);
}

float CStatusBar::RaceTimeWidth()
{
	return GetDurationWidth(m_CurrentRaceTime);
}
void CStatusBar::RaceTimeRender()
{
	int RaceTime = 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
	{
		RaceTime = m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit * 60 - ((Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed());

		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
			RaceTime = 0;
	}
	else if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
		RaceTime = (Client()->GameTick(g_Config.m_ClDummy) + m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer) / Client()->GameTickSpeed();
	else
		RaceTime = (Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed();
	m_CurrentRaceTime = RaceTime;
	char aTimeBuf[64];
	str_time((int64_t)RaceTime * 100, TIME_DAYS, aTimeBuf, sizeof(aTimeBuf));

	if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && RaceTime <= 60 && (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
	{
		const float Alpha = RaceTime <= 10 && (2 * time() / time_freq()) % 2 ? 0.5f : 1.0f;
		TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
	}
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aTimeBuf);
	TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClStatusBarTextColor)));
}

float CStatusBar::FPSWidth()
{
	return TextRender()->TextWidth(m_FontSize, "00000");
}
void CStatusBar::FPSRender()
{
	m_FrameTimeAverage = m_FrameTimeAverage * 0.9f + Client()->RenderFrameTime() * 0.1f;
	int FPS = (int)(1.0f / m_FrameTimeAverage + 0.5f);
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%d", FPS);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::PositionWidth()
{
	if(!m_pClient->m_Snap.m_apPlayerInfos[m_PlayerId])
		return 0.0f;

	return TextRender()->TextWidth(m_FontSize, "-0000.00, -0000.00");
}
void CStatusBar::PositionRender()
{
	const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_Prev;
	const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_Cur;
	const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	// To make the player position relative to blocks we need to divide by the block size
	const vec2 Pos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%2.2f, %2.2f", Pos.x, Pos.y);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::VelocityWidth()
{
	if(!m_pClient->m_Snap.m_apPlayerInfos[m_PlayerId])
		return 0.0f;

	return TextRender()->TextWidth(m_FontSize, "+00.00, +00.00");
}
void CStatusBar::VelocityRender()
{
	const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_Prev;
	const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[m_PlayerId].m_Cur;
	const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	const vec2 Vel = mix(vec2(pPrevChar->m_VelX, pPrevChar->m_VelY), vec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);
	float VelspeedX = Vel.x / 256.0f * Client()->GameTickSpeed();
	if(Vel.x >= -1 && Vel.x <= 1)
		VelspeedX = 0;
	float VelspeedY = Vel.y / 256.0f * Client()->GameTickSpeed();
	if(Vel.y >= -128 && Vel.y <= 128)
		VelspeedY = 0;
	float DisplaySpeedX = VelspeedX / 32;
	float VelspeedLength = length(vec2(Vel.x, Vel.y) / 256.0f) * Client()->GameTickSpeed();
	float Ramp = VelocityRamp(VelspeedLength, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
	DisplaySpeedX *= Ramp;
	float DisplaySpeedY = VelspeedY / 32;

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%+06.2f, %+06.2f", DisplaySpeedX, DisplaySpeedY);
	if(DisplaySpeedX > 100 || DisplaySpeedY > 100)
		str_format(aBuf, sizeof(aBuf), "%+06.2f, %+06.2f", DisplaySpeedX, DisplaySpeedY);

	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::ZoomWidth() { return TextRender()->TextWidth(m_FontSize, "00.00"); }
void CStatusBar::ZoomRender()
{
	char aBuf[32];
	const double ZoomStep = std::cos((30.0f * pi) / 180.0f);
	const float ConsoleZoom = std::log(m_pClient->m_Camera.m_Zoom * std::pow(ZoomStep, 10)) / std::log(ZoomStep);
	str_format(aBuf, sizeof(aBuf), "%.2f", ConsoleZoom);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}

float CStatusBar::SpaceWidth() { return 0.0f; }
void CStatusBar::SpaceRender() {}

void CStatusBar::UpdateStatusBarSize()
{
	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	m_BarHeight = g_Config.m_ClStatusBarHeight;
	m_Margin = m_BarHeight * 0.2f;
	m_BarY = m_Height - m_BarHeight;
	m_FontSize = m_BarHeight - (m_Margin * 2);
	m_Margin *= 1.5f;
}
void CStatusBar::OnInit()
{
	UpdateStatusBarSize();
	ApplyStatusBarScheme(g_Config.m_ClStatusBarScheme);
}

void CStatusBar::LabelRender(const char *pLabel)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:", pLabel);
	TextRender()->Text(m_CursorX, m_CursorY, m_FontSize, aBuf);
}
float CStatusBar::LabelWidth(const char *pLabel)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", pLabel);
	return TextRender()->TextWidth(m_FontSize, aBuf);
}

void CStatusBar::ApplyStatusBarScheme(const char *pScheme)
{
	m_StatusBarItems.clear();
	for(int i = 0; pScheme[i] != '\0'; ++i)
	{
		char letter = pScheme[i];
		for(auto &itemType : m_StatusItemTypes)
		{
			for(int l = 0; l < STATUSBAR_TYPE_LETTERS; ++l)
			{
				if(itemType.m_aLetters[l] == letter)
				{
					m_StatusBarItems.push_back(&itemType);
					break;
				}
			}
		}
	}
}

void CStatusBar::UpdateStatusBarScheme(char *pScheme)
{
	int index = 0;
	for(const auto &item : m_StatusBarItems)
	{
		if(index >= STATUSBAR_MAX_SIZE)
		{
			break;
		}
		pScheme[index++] = item->m_aLetters[0];
	}
	pScheme[index] = '\0';
}

void CStatusBar::OnRender()
{
	m_PingActive = false;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!g_Config.m_ClStatusBar || !m_pClient->m_Snap.m_pGameInfoObj)
		return;

	m_PlayerId = m_pClient->m_Snap.m_LocalClientId;
	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		m_PlayerId = m_pClient->m_Snap.m_SpecInfo.m_SpectatorId;

	UpdateStatusBarSize();

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
	Graphics()->DrawRect(m_BarX, m_BarY, m_Width, m_BarHeight, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClStatusBarColor)).WithAlpha(g_Config.m_ClStatusBarAlpha / 100.0f), 0, 0);
	TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClStatusBarTextColor)).WithAlpha(g_Config.m_ClStatusBarTextAlpha / 100.0f));

	//	std::vector<CStatusItem *> m_StatusBarItems = {&m_LocalTime, &m_LocalTime, &m_Space, &m_LocalTime};
	int SpaceCount = 0;
	const int ItemCount = (int)m_StatusBarItems.size();
	float UsedWidth = 0.0f;
	float AvailableWidth = m_Width - m_Margin * 2.0f; // 1 extra margin on the sides
	// Count the number of spaces and determine how much unused space there is
	for(const CStatusItem *Item : m_StatusBarItems)
	{
		if(str_comp(Item->m_aName, "Space") == 0)
			++SpaceCount;
		else
		{
			float ItemWidth = Item->GetWidth();
			if(g_Config.m_ClStatusBarLabels && Item->m_ShowLabel && ItemWidth > 0.0f)
				ItemWidth += LabelWidth(Item->m_aDisplayName);

			UsedWidth += ItemWidth;
		}
	}
	UsedWidth += m_Margin * (ItemCount + 1);
	AvailableWidth -= UsedWidth;
	// AvailableWidth can be negative so might as well not make it even worse
	float SpaceWidth = std::max((AvailableWidth) / (float)SpaceCount, 0.0f);

	float SpaceBetweenItems = std::max(AvailableWidth / (float)(ItemCount - 1), 0.0f);
	if(SpaceCount > 0)
		SpaceBetweenItems = 0;

	m_CursorY = m_BarY + (m_BarHeight - m_FontSize) / 2;

	m_CursorX = m_Margin;
	// Render items
	for(const CStatusItem *Item : m_StatusBarItems)
	{
		m_CursorX += m_Margin;
		float ItemWidth = Item->GetWidth();

		if(ItemWidth > 0.0f)
		{
			if(g_Config.m_ClStatusBarLabels && Item->m_ShowLabel)
			{
				LabelRender(Item->m_aDisplayName);
				m_CursorX += LabelWidth(Item->m_aDisplayName);
			}
			Item->RenderItem();
		}

		m_CursorX += ItemWidth;
		if(str_comp(Item->m_aName, "Space") == 0)
			m_CursorX += SpaceWidth;

		m_CursorX += SpaceBetweenItems;
	}
}
