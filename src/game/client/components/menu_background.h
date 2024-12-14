#ifndef GAME_CLIENT_COMPONENTS_MENU_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_MENU_BACKGROUND_H

#include <game/client/components/background.h>
#include <game/client/components/camera.h>

#include <array>
#include <chrono>
#include <string>
#include <vector>

class CMenuMap : public CBackgroundEngineMap
{
	MACRO_INTERFACE("menu_enginemap")
};

// themes
class CTheme
{
public:
	CTheme() {}
	CTheme(const char *pName, bool HasDay, bool HasNight) :
		m_Name(pName), m_HasDay(HasDay), m_HasNight(HasNight) {}

	std::string m_Name;
	bool m_HasDay;
	bool m_HasNight;
	IGraphics::CTextureHandle m_IconTexture;
	bool operator<(const CTheme &Other) const { return m_Name < Other.m_Name; }
};

class CMenuBackground : public CBackground
{
	std::chrono::nanoseconds m_ThemeScanStartTime{0};

public:
	enum
	{
		POS_START = 0,
		POS_DEMOS,
		POS_NEWS,
		POS_SETTINGS_LANGUAGE,
		POS_SETTINGS_GENERAL,
		POS_SETTINGS_PLAYER,
		POS_SETTINGS_TEE,
		POS_SETTINGS_APPEARANCE,
		POS_SETTINGS_CONTROLS,
		POS_SETTINGS_GRAPHICS,
		POS_SETTINGS_SOUND,
		POS_SETTINGS_DDNET,
		POS_SETTINGS_ASSETS,
		POS_SETTINGS_RESERVED0,
		POS_SETTINGS_RESERVED1,
		POS_BROWSER_INTERNET,
		POS_BROWSER_LAN,
		POS_BROWSER_FAVORITES,
		POS_BROWSER_CUSTOM0,
		POS_BROWSER_CUSTOM1,
		POS_BROWSER_CUSTOM2,
		POS_BROWSER_CUSTOM3,
		POS_BROWSER_CUSTOM4,
		POS_RESERVED0,
		POS_RESERVED1,
		POS_RESERVED2,

		NUM_POS,

		POS_BROWSER_CUSTOM_NUM = (POS_BROWSER_CUSTOM4 - POS_BROWSER_CUSTOM0) + 1,
		POS_SETTINGS_RESERVED_NUM = (POS_SETTINGS_RESERVED1 - POS_SETTINGS_RESERVED0) + 1,
		POS_RESERVED_NUM = (POS_RESERVED2 - POS_RESERVED0) + 1,
	};

	enum
	{
		PREDEFINED_THEMES_COUNT = 3,
	};

private:
	CCamera m_Camera;

	CBackgroundEngineMap *CreateBGMap() override;

	vec2 m_RotationCenter;
	std::array<vec2, NUM_POS> m_aPositions;
	int m_CurrentPosition;
	vec2 m_CurrentDirection = vec2(1.0f, 0.0f);
	vec2 m_AnimationStartPos;
	bool m_ChangedPosition;
	float m_MoveTime;

	bool m_IsInit;
	bool m_Loading;

	void ResetPositions();

	void LoadThemeIcon(CTheme &Theme);
	static int ThemeScan(const char *pName, int IsDir, int DirType, void *pUser);

	std::vector<CTheme> m_vThemes;

public:
	CMenuBackground();
	~CMenuBackground() override {}
	virtual int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnMapLoad() override;
	void OnRender() override;

	void LoadMenuBackground(bool HasDayHint = true, bool HasNightHint = true);

	bool Render();
	bool IsLoading() const { return m_Loading; }

	class CCamera *GetCurCamera() override;
	const char *LoadingTitle() const override;

	void ChangePosition(int PositionNumber);

	std::vector<CTheme> &GetThemes();
};

std::array<vec2, CMenuBackground::NUM_POS> GenerateMenuBackgroundPositions();

#endif
