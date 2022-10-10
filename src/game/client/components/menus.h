/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_H

#include <base/types.h>
#include <base/vmath.h>

#include <chrono>
#include <unordered_set>
#include <vector>

#include <engine/console.h>
#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/components/mapimages.h>

#include <game/client/component.h>
#include <game/client/ui.h>
#include <game/voting.h>

#include <game/client/render.h>

struct CServerProcess
{
	PROCESS Process;
	bool Initialized;
	CLineReader LineReader;
};

struct SColorPicker
{
public:
	const float ms_Width = 160.0f;
	const float ms_Height = 186.0f;

	float m_X;
	float m_Y;

	bool m_Active;

	CUIRect m_AttachedRect;
	unsigned int *m_pColor;
	unsigned int m_HSVColor;
};

// compnent to fetch keypresses, override all other input
class CMenusKeyBinder : public CComponent
{
public:
	bool m_TakeKey;
	bool m_GotKey;
	IInput::CEvent m_Key;
	int m_ModifierCombination;
	CMenusKeyBinder();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual bool OnInput(IInput::CEvent Event) override;
};

class CMenus : public CComponent
{
	static ColorRGBA ms_GuiColor;
	static ColorRGBA ms_ColorTabbarInactiveOutgame;
	static ColorRGBA ms_ColorTabbarActiveOutgame;
	static ColorRGBA ms_ColorTabbarHoverOutgame;
	static ColorRGBA ms_ColorTabbarInactiveIngame;
	static ColorRGBA ms_ColorTabbarActiveIngame;
	static ColorRGBA ms_ColorTabbarHoverIngame;
	static ColorRGBA ms_ColorTabbarInactive;
	static ColorRGBA ms_ColorTabbarActive;
	static ColorRGBA ms_ColorTabbarHover;

	static SColorPicker ms_ColorPicker;
	static bool ms_ValueSelectorTextMode;

	char m_aLocalStringHelper[1024];

	int DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, bool Enabled = true);
	int DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active);
	int DoButton_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName = nullptr, int Corners = IGraphics::CORNER_ALL, float r = 5.0f, float FontFactor = 0.0f, vec4 ColorHot = vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4 Color = vec4(1, 1, 1, 0.5f), int AlignVertically = 1, bool CheckForActiveColorPicker = false);
	int DoButton_MenuTab(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, SUIAnimator *pAnimator = nullptr, const ColorRGBA *pDefaultColor = nullptr, const ColorRGBA *pActiveColor = nullptr, const ColorRGBA *pHoverColor = nullptr, float EdgeRounding = 10, int AlignVertically = 1);

	int DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect);
	int DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_CheckBoxAutoVMarginAndSet(const void *pID, const char *pText, int *pValue, CUIRect *pRect, float VMargin);
	int DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	ColorHSLA DoLine_ColorPicker(CButtonContainer *pResetID, float LineSize, float WantedPickerPosition, float LabelSize, float BottomMargin, CUIRect *pMainRect, const char *pText, unsigned int *pColorValue, ColorRGBA DefaultColor, bool CheckBoxSpacing = true, bool UseCheckBox = false, int *pCheckBoxValue = nullptr);
	void DoLaserPreview(const CUIRect *pRect, ColorHSLA OutlineColor, ColorHSLA InnerColor, const int LaserType);
	int DoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, bool UseScroll, int Current, int Min, int Max, int Step, float Scale, bool IsHex, float Round, ColorRGBA *pColor);
	int DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	void DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoKeyReader(void *pID, const CUIRect *pRect, int Key, int ModifierCombination, int *pNewModifierCombination);

	void DoSettingsControlsButtons(int Start, int Stop, CUIRect View);

	float RenderSettingsControlsJoystick(CUIRect View);
	void DoJoystickAxisPicker(CUIRect View);
	void DoJoystickBar(const CUIRect *pRect, float Current, float Tolerance, bool Active);

	void RenderColorPicker();

	void RefreshSkins();

	// new gui with gui elements
	template<typename T>
	int DoButtonMenu(CUIElement &UIElement, const void *pID, T &&GetTextLambda, int Checked, const CUIRect *pRect, bool HintRequiresStringCheck, bool HintCanChangePositionOrSize = false, int Corners = IGraphics::CORNER_ALL, float r = 5.0f, float FontFactor = 0.0f, vec4 ColorHot = vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4 Color = vec4(1, 1, 1, 0.5f), int AlignVertically = 1)
	{
		CUIRect Text = *pRect;
		Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
		Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);

		if(!UIElement.AreRectsInit() || HintRequiresStringCheck || HintCanChangePositionOrSize || UIElement.Rect(0)->m_UITextContainer == -1)
		{
			bool NeedsRecalc = !UIElement.AreRectsInit() || UIElement.Rect(0)->m_UITextContainer == -1;
			if(HintCanChangePositionOrSize)
			{
				if(UIElement.AreRectsInit())
				{
					if(UIElement.Rect(0)->m_X != pRect->x || UIElement.Rect(0)->m_Y != pRect->y || UIElement.Rect(0)->m_Width != pRect->w || UIElement.Rect(0)->m_Y != pRect->h)
					{
						NeedsRecalc = true;
					}
				}
			}
			const char *pText = nullptr;
			if(HintRequiresStringCheck)
			{
				if(UIElement.AreRectsInit())
				{
					pText = GetTextLambda();
					if(str_comp(UIElement.Rect(0)->m_Text.c_str(), pText) != 0)
					{
						NeedsRecalc = true;
					}
				}
			}
			if(NeedsRecalc)
			{
				if(!UIElement.AreRectsInit())
				{
					UIElement.InitRects(3);
				}
				UI()->ResetUIElement(UIElement);

				vec4 RealColor = Color;
				for(int i = 0; i < 3; ++i)
				{
					Color.a = RealColor.a;
					if(i == 0)
						Color.a *= UI()->ButtonColorMulActive();
					else if(i == 1)
						Color.a *= UI()->ButtonColorMulHot();
					else if(i == 2)
						Color.a *= UI()->ButtonColorMulDefault();
					Graphics()->SetColor(Color);

					CUIElement::SUIElementRect &NewRect = *UIElement.Rect(i);
					NewRect.m_UIRectQuadContainer = Graphics()->CreateRectQuadContainer(pRect->x, pRect->y, pRect->w, pRect->h, r, Corners);

					NewRect.m_X = pRect->x;
					NewRect.m_Y = pRect->y;
					NewRect.m_Width = pRect->w;
					NewRect.m_Height = pRect->h;
					if(i == 0)
					{
						if(pText == nullptr)
							pText = GetTextLambda();
						NewRect.m_Text = pText;
						SLabelProperties Props;
						Props.m_AlignVertically = AlignVertically;
						UI()->DoLabel(NewRect, &Text, pText, Text.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
					}
				}
				Graphics()->SetColor(1, 1, 1, 1);
			}
		}
		// render
		size_t Index = 2;
		if(UI()->CheckActiveItem(pID))
			Index = 0;
		else if(UI()->HotItem() == pID)
			Index = 1;
		Graphics()->TextureClear();
		Graphics()->RenderQuadContainer(UIElement.Rect(Index)->m_UIRectQuadContainer, -1);
		ColorRGBA ColorText(TextRender()->DefaultTextColor());
		ColorRGBA ColorTextOutline(TextRender()->DefaultTextOutlineColor());
		if(UIElement.Rect(0)->m_UITextContainer != -1)
			TextRender()->RenderTextContainer(UIElement.Rect(0)->m_UITextContainer, ColorText, ColorTextOutline);
		return UI()->DoButtonLogic(pID, Checked, pRect);
	}

	struct CListboxItem
	{
		int m_Visible;
		int m_Selected;
		CUIRect m_Rect;
		CUIRect m_HitRect;
	};

	void UiDoListboxStart(const void *pID, const CUIRect *pRect, float RowHeight, const char *pTitle, const char *pBottomText, int NumItems,
		int ItemsPerRow, int SelectedIndex, float ScrollValue, bool LogicOnly = false);
	CListboxItem UiDoListboxNextItem(const void *pID, bool Selected = false, bool KeyEvents = true, bool NoHoverEffects = false);
	CListboxItem UiDoListboxNextRow();
	int UiDoListboxEnd(float *pScrollValue, bool *pItemActivated, bool *pListBoxActive = nullptr);

	int UiLogicGetCurrentClickedItem();

	/**
	 * Places and renders a tooltip near pNearRect.
	 * For now only works correctly with single line tooltips, since Text width calculation gets broken when there are multiple lines.
	 *
	 * @param pID The ID of the tooltip. Usually a reference to some g_Config value.
	 * @param pNearTo Place the tooltip near this rect.
	 * @param pText The text to display in the tooltip
	 */
	void DoToolTip(const void *pID, const CUIRect *pNearRect, const char *pText, float WidthHint = -1.0f);
	// menus_settings_assets.cpp
public:
	struct SCustomItem
	{
		IGraphics::CTextureHandle m_RenderTexture;

		char m_aName[50];

		bool operator<(const SCustomItem &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
	};

	struct SCustomEntities : public SCustomItem
	{
		struct SEntitiesImage
		{
			IGraphics::CTextureHandle m_Texture;
		};
		SEntitiesImage m_aImages[MAP_IMAGE_MOD_TYPE_COUNT];
	};

	struct SCustomGame : public SCustomItem
	{
	};

	struct SCustomEmoticon : public SCustomItem
	{
	};

	struct SCustomParticle : public SCustomItem
	{
	};

	struct SCustomHud : public SCustomItem
	{
	};

	struct SCustomExtras : public SCustomItem
	{
	};

protected:
	std::vector<SCustomEntities> m_vEntitiesList;
	std::vector<SCustomGame> m_vGameList;
	std::vector<SCustomEmoticon> m_vEmoticonList;
	std::vector<SCustomParticle> m_vParticlesList;
	std::vector<SCustomHud> m_vHudList;
	std::vector<SCustomExtras> m_vExtrasList;

	bool m_IsInit = false;

	static void LoadEntities(struct SCustomEntities *pEntitiesItem, void *pUser);
	static int EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser);

	static int GameScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int HudScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ExtrasScan(const char *pName, int IsDir, int DirType, void *pUser);

	static void ConchainAssetsEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetGame(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetParticles(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetEmoticons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetExtras(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void ClearCustomItems(int CurTab);

	int m_MenuPage;
	int m_GamePage;
	int m_Popup;
	int m_ActivePage;
	bool m_ShowStart;
	bool m_MenuActive;
	vec2 m_MousePos;
	bool m_JoinTutorial;

	char m_aNextServer[256];

	// images
	struct CMenuImage
	{
		char m_aName[64];
		IGraphics::CTextureHandle m_OrgTexture;
		IGraphics::CTextureHandle m_GreyTexture;
	};
	std::vector<CMenuImage> m_vMenuImages;

	static int MenuImageScan(const char *pName, int IsDir, int DirType, void *pUser);

	const CMenuImage *FindMenuImage(const char *pName);

	// loading
	int m_LoadCurrent;
	int m_LoadTotal;

	//
	char m_aMessageTopic[512];
	char m_aMessageBody[512];
	char m_aMessageButton[512];

	CUIElement m_RefreshButton;
	CUIElement m_ConnectButton;

	void PopupMessage(const char *pTopic, const char *pBody, const char *pButton);

	// TODO: this is a bit ugly but.. well.. yeah
	enum
	{
		MAX_INPUTEVENTS = 32
	};
	static IInput::CEvent m_aInputEvents[MAX_INPUTEVENTS];
	static int m_NumInputEvents;

	// some settings
	static float ms_ButtonHeight;
	static float ms_ListheaderHeight;
	static float ms_ListitemAdditionalHeight;

	// for settings
	bool m_NeedRestartGeneral;
	bool m_NeedRestartSkins;
	bool m_NeedRestartGraphics;
	bool m_NeedRestartSound;
	bool m_NeedRestartUpdate;
	bool m_NeedRestartDDNet;
	bool m_NeedSendinfo;
	bool m_NeedSendDummyinfo;
	int m_SettingPlayerPage;

	// for map download popup
	int64_t m_DownloadLastCheckTime;
	int m_DownloadLastCheckSize;
	float m_DownloadSpeed;

	// for call vote
	int m_CallvoteSelectedOption;
	int m_CallvoteSelectedPlayer;
	char m_aCallvoteReason[VOTE_REASON_LENGTH];
	char m_aFilterString[25];
	bool m_ControlPageOpening;

	// demo
	enum
	{
		SORT_DEMONAME = 0,
		SORT_MARKERS,
		SORT_LENGTH,
		SORT_DATE,
	};

	struct CDemoItem
	{
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aName[128];
		bool m_IsDir;
		int m_StorageType;
		time_t m_Date;

		bool m_InfosLoaded;
		bool m_Valid;
		CDemoHeader m_Info;
		CTimelineMarkers m_TimelineMarkers;
		CMapInfo m_MapInfo;

		int NumMarkers() const
		{
			return clamp<int>(bytes_be_to_int(m_TimelineMarkers.m_aNumTimelineMarkers), 0, MAX_TIMELINE_MARKERS);
		}

		int Length() const
		{
			return bytes_be_to_int(m_Info.m_aLength);
		}

		unsigned Size() const
		{
			return bytes_be_to_uint(m_Info.m_aMapSize);
		}

		bool operator<(const CDemoItem &Other) const
		{
			if(!str_comp(m_aFilename, ".."))
				return true;
			if(!str_comp(Other.m_aFilename, ".."))
				return false;
			if(m_IsDir && !Other.m_IsDir)
				return true;
			if(!m_IsDir && Other.m_IsDir)
				return false;

			const CDemoItem &Left = g_Config.m_BrDemoSortOrder ? Other : *this;
			const CDemoItem &Right = g_Config.m_BrDemoSortOrder ? *this : Other;

			if(g_Config.m_BrDemoSort == SORT_DEMONAME)
				return str_comp_filenames(Left.m_aFilename, Right.m_aFilename) < 0;
			if(g_Config.m_BrDemoSort == SORT_DATE)
				return Left.m_Date < Right.m_Date;

			if(!Other.m_InfosLoaded)
				return m_InfosLoaded;
			if(!m_InfosLoaded)
				return !Other.m_InfosLoaded;

			if(g_Config.m_BrDemoSort == SORT_MARKERS)
				return Left.NumMarkers() < Right.NumMarkers();
			if(g_Config.m_BrDemoSort == SORT_LENGTH)
				return Left.Length() < Right.Length();

			// Unknown sort
			return true;
		}
	};

	char m_aCurrentDemoFolder[256];
	char m_aCurrentDemoFile[64];
	int m_DemolistSelectedIndex;
	bool m_DemolistSelectedIsDir;
	int m_DemolistStorageType;
	int m_Speed = 4;

	std::chrono::nanoseconds m_DemoPopulateStartTime{0};

	void DemolistOnUpdate(bool Reset);
	//void DemolistPopulate();
	static int DemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);

	// friends
	struct CFriendItem
	{
		const CFriendInfo *m_pFriendInfo;
		int m_NumFound;

		CFriendItem() {}
		CFriendItem(const CFriendInfo *pFriendInfo) :
			m_pFriendInfo(pFriendInfo), m_NumFound(0)
		{
		}

		bool operator<(const CFriendItem &Other) const
		{
			if(m_NumFound && !Other.m_NumFound)
				return true;
			else if(!m_NumFound && Other.m_NumFound)
				return false;
			else
			{
				int Result = str_comp(m_pFriendInfo->m_aName, Other.m_pFriendInfo->m_aName);
				if(Result)
					return Result < 0;
				else
					return str_comp(m_pFriendInfo->m_aClan, Other.m_pFriendInfo->m_aClan) < 0;
			}
		}
	};

	std::vector<CFriendItem> m_vFriends;
	int m_FriendlistSelectedIndex;

	void FriendlistOnUpdate();

	// found in menus.cpp
	int Render();
	//void render_background();
	//void render_loading(float percent);
	int RenderMenubar(CUIRect r);
	void RenderNews(CUIRect MainView);
	static void ConchainUpdateMusicState(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	void UpdateMusicState();

	// found in menus_demo.cpp
	static bool DemoFilterChat(const void *pData, int Size, void *pUser);
	bool FetchHeader(CDemoItem &Item);
	void FetchAllHeaders();
	void HandleDemoSeeking(float PositionToSeek, float TimeToSeek);
	void RenderDemoPlayer(CUIRect MainView);
	void RenderDemoList(CUIRect MainView);

	// found in menus_start.cpp
	void RenderStartMenu(CUIRect MainView);

	// found in menus_ingame.cpp
	void RenderGame(CUIRect MainView);
	void RenderPlayers(CUIRect MainView);
	void RenderServerInfo(CUIRect MainView);
	void RenderServerControl(CUIRect MainView);
	bool RenderServerControlKick(CUIRect MainView, bool FilterSpectators);
	bool RenderServerControlServer(CUIRect MainView);
	void RenderIngameHint();

	// found in menus_browser.cpp
	int m_SelectedIndex;
	int m_DoubleClickIndex;
	int m_ScrollOffset;
	void RenderServerbrowserServerList(CUIRect View);
	void RenderServerbrowserServerDetail(CUIRect View);
	void RenderServerbrowserFilters(CUIRect View);
	void RenderServerbrowserFriends(CUIRect View);
	void RenderServerbrowser(CUIRect MainView);
	static void ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainServerbrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// skin favorite list
	bool m_SkinFavoritesChanged = false;
	std::unordered_set<std::string> m_SkinFavorites;
	static void Con_AddFavoriteSkin(IConsole::IResult *pResult, void *pUserData);
	static void Con_RemFavoriteSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
	void OnConfigSave(IConfigManager *pConfigManager);

	// found in menus_settings.cpp
	void RenderLanguageSelection(CUIRect MainView);
	void RenderThemeSelection(CUIRect MainView, bool Header = true);
	void RenderSettingsGeneral(CUIRect MainView);
	void RenderSettingsPlayer(CUIRect MainView);
	void RenderSettingsDummyPlayer(CUIRect MainView);
	void RenderSettingsTee(CUIRect MainView);
	void RenderSettingsControls(CUIRect MainView);
	void RenderSettingsGraphics(CUIRect MainView);
	void RenderSettingsSound(CUIRect MainView);
	void RenderSettings(CUIRect MainView);
	void RenderSettingsCustom(CUIRect MainView);

	void SetNeedSendInfo();
	void SetActive(bool Active);

	IGraphics::CTextureHandle m_TextureBlob;

	bool CheckHotKey(int Key) const;

	class CMenuBackground *m_pBackground;

public:
	void RenderBackground();

	void SetMenuBackground(class CMenuBackground *pBackground) { m_pBackground = pBackground; }

	static CMenusKeyBinder m_Binder;

	CMenus();
	virtual int Sizeof() const override { return sizeof(*this); }

	void RenderLoading(const char *pCaption, const char *pContent, int IncreaseCounter, bool RenderLoadingBar = true, bool RenderMenuBackgroundMap = true);

	bool IsInit() { return m_IsInit; }

	bool IsActive() const { return m_MenuActive; }
	void KillServer();

	virtual void OnInit() override;
	void OnConsoleInit() override;

	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual bool OnInput(IInput::CEvent Event) override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	virtual void OnShutdown() override;

	enum
	{
		PAGE_NEWS = 1,
		PAGE_GAME,
		PAGE_PLAYERS,
		PAGE_SERVER_INFO,
		PAGE_CALLVOTE,
		PAGE_INTERNET,
		PAGE_LAN,
		PAGE_FAVORITES,
		PAGE_DDNET,
		PAGE_KOG,
		PAGE_DEMOS,
		PAGE_SETTINGS,
		PAGE_SYSTEM,
		PAGE_NETWORK,
		PAGE_GHOST,

		PAGE_LENGTH,

		SETTINGS_LANGUAGE = 0,
		SETTINGS_GENERAL,
		SETTINGS_PLAYER,
		SETTINGS_TEE,
		SETTINGS_APPEARANCE,
		SETTINGS_CONTROLS,
		SETTINGS_GRAPHICS,
		SETTINGS_SOUND,
		SETTINGS_DDNET,
		SETTINGS_ASSETS,

		SETTINGS_LENGTH,

		BIG_TAB_NEWS = 0,
		BIG_TAB_INTERNET,
		BIG_TAB_LAN,
		BIG_TAB_FAVORITES,
		BIG_TAB_DDNET,
		BIG_TAB_KOG,
		BIG_TAB_DEMOS,

		BIG_TAB_LENGTH,

		SMALL_TAB_HOME = 0,
		SMALL_TAB_QUIT,
		SMALL_TAB_SETTINGS,
		SMALL_TAB_EDITOR,
		SMALL_TAB_DEMOBUTTON,
		SMALL_TAB_SERVER,

		SMALL_TAB_LENGTH,
	};

	SUIAnimator m_aAnimatorsBigPage[BIG_TAB_LENGTH];
	SUIAnimator m_aAnimatorsSmallPage[SMALL_TAB_LENGTH];
	SUIAnimator m_aAnimatorsSettingsTab[SETTINGS_LENGTH];

	// DDRace
	int DoButton_CheckBox_Tristate(const void *pID, const char *pText, TRISTATE Checked, const CUIRect *pRect);
	std::vector<CDemoItem> m_vDemos;
	void DemolistPopulate();
	void DemoSeekTick(IDemoPlayer::ETickOffset TickOffset);
	bool m_Dummy;

	const char *GetCurrentDemoFolder() const { return m_aCurrentDemoFolder; }

	// Ghost
	struct CGhostItem
	{
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aPlayer[MAX_NAME_LENGTH];

		int m_Time;
		int m_Slot;
		bool m_Own;

		CGhostItem() :
			m_Slot(-1), m_Own(false) { m_aFilename[0] = 0; }

		bool operator<(const CGhostItem &Other) const { return m_Time < Other.m_Time; }

		bool Active() const { return m_Slot != -1; }
		bool HasFile() const { return m_aFilename[0]; }
	};

	std::vector<CGhostItem> m_vGhosts;

	std::chrono::nanoseconds m_GhostPopulateStartTime{0};

	void GhostlistPopulate();
	CGhostItem *GetOwnGhost();
	void UpdateOwnGhost(CGhostItem Item);
	void DeleteGhostItem(int Index);

	void setPopup(int Popup) { m_Popup = Popup; }
	int GetCurPopup() { return m_Popup; }
	bool CanDisplayWarning();

	void PopupWarning(const char *pTopic, const char *pBody, const char *pButton, std::chrono::nanoseconds Duration);

	std::chrono::nanoseconds m_PopupWarningLastTime;
	std::chrono::nanoseconds m_PopupWarningDuration;

	int m_DemoPlayerState;
	char m_aDemoPlayerPopupHint[256];

	enum
	{
		POPUP_NONE = 0,
		POPUP_FIRST_LAUNCH,
		POPUP_POINTS,
		POPUP_CONNECTING,
		POPUP_MESSAGE,
		POPUP_DISCONNECTED,
		POPUP_LANGUAGE,
		POPUP_COUNTRY,
		POPUP_DELETE_DEMO,
		POPUP_RENAME_DEMO,
		POPUP_RENDER_DEMO,
		POPUP_REPLACE_VIDEO,
		POPUP_REMOVE_FRIEND,
		POPUP_SOUNDERROR,
		POPUP_PASSWORD,
		POPUP_QUIT,
		POPUP_DISCONNECT,
		POPUP_DISCONNECT_DUMMY,
		POPUP_WARNING,
		POPUP_SWITCH_SERVER,

		// demo player states
		DEMOPLAYER_NONE = 0,
		DEMOPLAYER_SLICE_SAVE,
	};

private:
	static int GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	void SetMenuPage(int NewPage);
	void RefreshBrowserTab(int UiPage);
	bool HandleListInputs(const CUIRect &View, float &ScrollValue, float ScrollAmount, int *pScrollOffset, float ElemHeight, int &SelectedIndex, int NumElems);

	// found in menus_ingame.cpp
	void RenderInGameNetwork(CUIRect MainView);
	void RenderGhost(CUIRect MainView);

	// found in menus_settings.cpp
	void RenderSettingsDDNet(CUIRect MainView);
	void RenderSettingsAppearance(CUIRect MainView);
	ColorHSLA RenderHSLColorPicker(const CUIRect *pRect, unsigned int *pColor, bool Alpha);
	ColorHSLA RenderHSLScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha = false, bool ClampedLight = false);

	int RenderDropDown(int &CurDropDownState, CUIRect *pRect, int CurSelection, const void **pIDs, const char **pStr, int PickNum, CButtonContainer *pButtonContainer, float &ScrollVal);

	CServerProcess m_ServerProcess;
};
#endif
