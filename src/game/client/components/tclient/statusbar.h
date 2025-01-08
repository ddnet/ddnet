#ifndef GAME_CLIENT_COMPONENTS_STATUSBAR_H
#define GAME_CLIENT_COMPONENTS_STATUSBAR_H
#include <game/client/component.h>

enum
{
	STATUSBAR_MAX_SIZE = 16,
	STATUSBAR_TYPE_LETTERS = 4
};

class CStatusItem
{
public:
	std::function<void()> RenderItem;
	std::function<float()> GetWidth;
	char m_aName[32];
	char m_aDisplayName[32];
	char m_aDesc[128];
	char m_aLetters[STATUSBAR_TYPE_LETTERS] = {};
	bool m_ShowLabel = true;
	CStatusItem(std::function<void()> Render, std::function<float()> Width, const char *pLetters, const char *pName, const char *pDisplayName, const char *pDesc, bool ShowLabel = true)
	{
		RenderItem = Render;
		GetWidth = Width;
		str_copy(m_aLetters, pLetters);
		str_copy(m_aName, pName);
		if(str_comp(pDisplayName, "") != 0)
			str_copy(m_aDisplayName, pDisplayName);
		else
			str_copy(m_aDisplayName, pName);
		str_copy(m_aDesc, pDesc);
		m_ShowLabel = ShowLabel;
	}
};

class CStatusBar : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnInit() override;

	CStatusItem m_Angle = CStatusItem(std::bind(&CStatusBar::AngleRender, this), std::bind(&CStatusBar::AngleWidth, this),
		"a", "Angle", "", "Displays your current angle in degrees");
	CStatusItem m_Ping = CStatusItem(std::bind(&CStatusBar::PingRender, this), std::bind(&CStatusBar::PingWidth, this),
		"p", "Ping", "", "Displays your ping to the current server");
	CStatusItem m_Prediction = CStatusItem(std::bind(&CStatusBar::PredictionRender, this), std::bind(&CStatusBar::PredictionWidth, this),
		"d", "Prediction", "Pred", "Displays your current prediction amount");
	CStatusItem m_Position = CStatusItem(std::bind(&CStatusBar::PositionRender, this), std::bind(&CStatusBar::PositionWidth, this),
		"c", "Position", "Pos", "Displays position");
	CStatusItem m_LocalTime = CStatusItem(std::bind(&CStatusBar::LocalTimeRender, this), std::bind(&CStatusBar::LocalTimeWidth, this),
		"l", "Local Time", "", "Displays your local time", false);
	CStatusItem m_RaceTime = CStatusItem(std::bind(&CStatusBar::RaceTimeRender, this), std::bind(&CStatusBar::RaceTimeWidth, this),
		"r", "Race Time", "", "Display your race time", false);
	CStatusItem m_FPS = CStatusItem(std::bind(&CStatusBar::FPSRender, this), std::bind(&CStatusBar::FPSWidth, this),
		"f", "FPS", "", "Displays your frames per second");
	CStatusItem m_Velocity = CStatusItem(std::bind(&CStatusBar::VelocityRender, this), std::bind(&CStatusBar::VelocityWidth, this),
		"v", "Velocity", "", "Displays X and Y velocity");
	CStatusItem m_Zoom = CStatusItem(std::bind(&CStatusBar::ZoomRender, this), std::bind(&CStatusBar::ZoomWidth, this),
		"z", "Zoom", "", "Displays current zoom value");
	CStatusItem m_Space = CStatusItem(std::bind(&CStatusBar::SpaceRender, this), std::bind(&CStatusBar::SpaceWidth, this),
		" _", "Space", " ", "Gap between statusbar items", false);

	std::vector<CStatusItem> m_StatusItemTypes = {m_Angle, m_Ping, m_Prediction, m_Position, m_LocalTime, m_RaceTime, m_FPS, m_Velocity, m_Zoom, m_Space};
	std::vector<CStatusItem *> m_StatusBarItems = {&m_LocalTime, &m_FPS, &m_Space, &m_Angle, &m_Space, &m_Ping};

	void UpdateStatusBarSize();
	void ApplyStatusBarScheme(const char *pScheme);
	void UpdateStatusBarScheme(char *pScheme);

	bool m_PingActive = false;

private:
	float m_FrameTimeAverage = 0.0f;
	int m_PlayerId = 0;
	float m_FontSize = 12.0f;
	float m_CursorX, m_CursorY, m_BarX = 0.0f, m_BarY;
	float m_Width, m_Height;
	float m_BarHeight, m_Margin;

	int m_CurrentRaceTime = 0;
	float GetDurationWidth(int Time);
	int GetDigitsIndex(const int Value, const int Max);
	float AngleWidth();
	void AngleRender();

	float PingWidth();
	void PingRender();

	float PredictionWidth();
	void PredictionRender();

	float PositionWidth();
	void PositionRender();

	float LocalTimeWidth();
	void LocalTimeRender();

	float RaceTimeWidth();
	void RaceTimeRender();

	float FPSWidth();
	void FPSRender();

	float VelocityWidth();
	void VelocityRender();

	float ZoomWidth();
	void ZoomRender();

	float SpaceWidth();
	void SpaceRender();

	void LabelRender(const char *pLabel);
	float LabelWidth(const char *pLabel);

	// float Width();
	// void Render();
	// float Width();
	// void Render();
};

#endif
