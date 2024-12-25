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
	return Duration >= 3600 * 24 * 100 ? s_TextWidth000D : Duration >= 3600 * 24 * 10 ? s_TextWidth00D :
						       Duration >= 3600 * 24              ? s_TextWidth0D :
						       Duration >= 3600                   ? s_TextWidthH :
											    s_TextWidthM;
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
void CStatusBar::PingRender() {

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
void CStatusBar::PredictionRender() {}

float CStatusBar::PositionWidth() { return 0.0f; }
void CStatusBar::PositionRender() {}

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

float CStatusBar::RaceTimeWidth() { return 0.0f; }
void CStatusBar::RaceTimeRender() {}

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

float CStatusBar::VelocityWidth() { return 0.0f; }
void CStatusBar::VelocityRender() {}

float CStatusBar::ZoomWidth() { return 0.0f; }
void CStatusBar::ZoomRender() {}

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
}

void CStatusBar::LabelRender(const char *pLabel) {}
float CStatusBar::LabelWidth(const char *pLabel) { return 0.0f; }

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

	//	std::vector<CStatusItem *> m_StatusBarList = {&m_LocalTime, &m_LocalTime, &m_Space, &m_LocalTime};
	int SpaceCount = 0;
	const int ItemCount = (int)m_StatusBarList.size();
	float UsedWidth = 0.0f;
	float AvailableWidth = m_Width - m_Margin * 2.0f; // 1 extra margin on the sides
	// Count the number of spaces and determine how much unused space there is
	for(const CStatusItem *Item : m_StatusBarList)
	{
		if(str_comp(Item->m_aName, "Space") == 0)
			++SpaceCount;
		else
			UsedWidth += Item->GetWidth();
	}
	UsedWidth += m_Margin * (ItemCount + 1);
	AvailableWidth -= UsedWidth;
	// AvailableWidth can be negative so might as well not make it even worse
	float SpaceWidth = std::max((AvailableWidth) / (float)SpaceCount, 0.0f);

	float SpaceBetweenItems = std::max(AvailableWidth / (float)ItemCount, 0.0f);
	if(SpaceCount > 0)
		SpaceBetweenItems = 0;

	m_CursorY = m_BarY + (m_BarHeight - m_FontSize) / 2;

	m_CursorX = m_Margin;
	// Render items
	for(const CStatusItem *Item : m_StatusBarList)
	{
		m_CursorX += m_Margin;
		float ItemWidth = Item->GetWidth();

		if(ItemWidth > 0.0f)
			Item->RenderItem();

		m_CursorX += ItemWidth;
		if(str_comp(Item->m_aName, "Space") == 0)
			m_CursorX += SpaceWidth;

		m_CursorX += SpaceBetweenItems;
	}
}
