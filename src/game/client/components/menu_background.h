#ifndef GAME_CLIENT_COMPONENTS_MENU_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_MENU_BACKGROUND_H

#include <game/client/components/background.h>
#include <game/client/components/camera.h>

#include <vector>

class CMenuMap : public CBackgroundEngineMap
{
	MACRO_INTERFACE("menu_enginemap", 0)
};

// themes
class CTheme
{
public:
	CTheme() {}
	CTheme(const char *n, bool HasDay, bool HasNight) :
		m_Name(n), m_HasDay(HasDay), m_HasNight(HasNight) {}

	string m_Name;
	bool m_HasDay;
	bool m_HasNight;
	IGraphics::CTextureHandle m_IconTexture;
	bool operator<(const CTheme &Other) const { return m_Name < Other.m_Name; }
};

class CMenuBackground : public CBackground
{
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
		POS_SETTINGS_HUD,
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
		POS_BROWSER_CUSTOM,
		POS_BROWSER_CUSTOM0 = POS_BROWSER_CUSTOM, // ddnet tab
		POS_BROWSER_CUSTOM1, // kog tab
		POS_BROWSER_CUSTOM2,
		POS_BROWSER_CUSTOM3,
		POS_RESERVED0,
		POS_RESERVED1,
		POS_RESERVED2,

		NUM_POS,

		POS_BROWSER_CUSTOM_NUM = (POS_BROWSER_CUSTOM3 - POS_BROWSER_CUSTOM0) + 1,
		POS_SETTINGS_RESERVED_NUM = (POS_SETTINGS_RESERVED1 - POS_SETTINGS_RESERVED0) + 1,
		POS_RESERVED_NUM = (POS_RESERVED2 - POS_RESERVED0) + 1,
	};

	enum
	{
		PREDEFINED_THEMES_COUNT = 3,
	};

	CCamera m_Camera;

	CBackgroundEngineMap *CreateBGMap() override;

	vec2 m_MenuCenter;
	vec2 m_RotationCenter;
	vec2 m_Positions[NUM_POS];
	int m_CurrentPosition;
	vec2 m_AnimationStartPos;
	bool m_ChangedPosition;
	float m_MoveTime;

	bool m_IsInit;

	void ResetPositions();

	static int ThemeScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ThemeIconScan(const char *pName, int IsDir, int DirType, void *pUser);

	std::vector<CTheme> m_lThemes;

public:
	CMenuBackground();
	~CMenuBackground() override {}

	void OnInit() override;
	void OnMapLoad() override;
	void OnRender() override;

	void LoadMenuBackground(bool HasDayHint = true, bool HasNightHint = true);

	bool Render();

	class CCamera *GetCurCamera() override;

	void ChangePosition(int PositionNumber);

	std::vector<CTheme> &GetThemes();
};

#endif
