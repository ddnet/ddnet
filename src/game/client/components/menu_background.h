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
	CTheme(const char *n, bool HasDay, bool HasNight)
		: m_Name(n), m_HasDay(HasDay), m_HasNight(HasNight) {}

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
		POS_INTERNET,
		POS_LAN,
		POS_DEMOS,
		POS_NEWS,
		POS_FAVORITES,
		POS_SETTINGS_LANGUAGE,
		POS_SETTINGS_GENERAL,
		POS_SETTINGS_PLAYER,
		POS_SETTINGS_TEE,
		POS_SETTINGS_HUD,
		POS_SETTINGS_CONTROLS,
		POS_SETTINGS_GRAPHICS,
		POS_SETTINGS_SOUND,
		POS_SETTINGS_DDNET,
		POS_CUSTOM,
		POS_CUSTOM0 = POS_CUSTOM, // ddnet tab
		POS_CUSTOM1, // kog tab
		POS_CUSTOM2,
		POS_CUSTOM3,
		POS_CUSTOM4,
		POS_CUSTOM5,
		POS_CUSTOM6,
		POS_CUSTOM7,
		POS_CUSTOM8,
		POS_CUSTOM9,

		NUM_POS,

		POS_CUSTOM_NUM = (POS_CUSTOM9 - POS_CUSTOM0) + 1,
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
