/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_H

#include <base/tl/sorted_array.h>
#include <base/vmath.h>

#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/components/mapimages.h>

#include <game/client/component.h>
#include <game/client/ui.h>
#include <game/voting.h>

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
	int m_Modifier;
	CMenusKeyBinder();
	virtual bool OnInput(IInput::CEvent Event);
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

	float ButtonColorMulActive() { return 0.5f; }
	float ButtonColorMulHot() { return 1.5f; }
	float ButtonColorMulDefault() { return 1.0f; }
	float ButtonColorMul(const void *pID);

	int DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_Sprite(const void *pID, int ImageID, int SpriteID, int Checked, const CUIRect *pRect, int Corners);
	int DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active);
	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, const char *pImageName = 0, int Corners = CUI::CORNER_ALL, float r = 5.0f, float FontFactor = 0.0f, vec4 ColorHot = vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4 Color = vec4(1, 1, 1, 0.5f), int AlignVertically = 1, bool CheckForActiveColorPicker = false);
	int DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners, SUIAnimator *pAnimator = NULL, const ColorRGBA *pDefaultColor = NULL, const ColorRGBA *pActiveColor = NULL, const ColorRGBA *pHoverColor = NULL, float EdgeRounding = 10, int AlignVertically = 1);

	int DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect);
	int DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_CheckBoxAutoVMarginAndSet(const void *pID, const char *pText, int *pValue, CUIRect *pRect, float VMargin);
	int DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	ColorHSLA DoLine_ColorPicker(int *pResetID, const float LineSize, const float WantedPickerPosition, const float LabelSize, const float BottomMargin, CUIRect *pMainRect, const char *pText, unsigned int *pColorValue, const ColorRGBA DefaultColor, bool CheckBoxSpacing = true, bool UseCheckBox = false, int *pCheckBoxValue = NULL);
	void DoLaserPreview(const CUIRect *pRect, const ColorHSLA OutlineColor, const ColorHSLA InnerColor);
	/*static void ui_draw_menu_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_keyselect_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_menu_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_settings_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/
	int DoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, bool UseScroll, int Current, int Min, int Max, int Step, float Scale, bool IsHex, float Round, ColorRGBA *Color);
	int DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect);
	int DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	//static void ui_draw_browse_icon(int what, const CUIRect *r);
	//static void ui_draw_grid_header(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);

	/*static void ui_draw_checkbox_common(const void *id, const char *text, const char *boxtext, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox_number(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/
	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const char *pEmptyText = "");
	int DoClearableEditBox(void *pID, void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const char *pEmptyText = "");
	//static int ui_do_edit_box(void *id, const CUIRect *rect, char *str, unsigned str_size, float font_size, bool hidden=false);

	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current);
	void DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoKeyReader(void *pID, const CUIRect *pRect, int Key, int Modifier, int *NewModifier);

	//static int ui_do_key_reader(void *id, const CUIRect *rect, int key);
	void UiDoGetButtons(int Start, int Stop, CUIRect View, CUIRect ScopeView);

	void RenderColorPicker();

	// new gui with gui elements
	template<typename T>
	int DoButtonMenu(CUIElement &UIElement, const void *pID, T &&GetTextLambda, int Checked, const CUIRect *pRect, bool HintRequiresStringCheck, bool HintCanChangePositionOrSize = false, int Corners = CUI::CORNER_ALL, float r = 5.0f, float FontFactor = 0.0f, vec4 ColorHot = vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4 Color = vec4(1, 1, 1, 0.5f), int AlignVertically = 1)
	{
		CUIRect Text = *pRect;
		Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
		Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);

		if(UIElement.Size() != 3 || HintRequiresStringCheck || HintCanChangePositionOrSize)
		{
			bool NeedsRecalc = UIElement.Size() != 3;
			if(HintCanChangePositionOrSize)
			{
				if(UIElement.Size() == 3)
				{
					if(UIElement.Get(0)->m_X != pRect->x || UIElement.Get(0)->m_Y != pRect->y || UIElement.Get(0)->m_Width != pRect->w || UIElement.Get(0)->m_Y != pRect->h)
					{
						NeedsRecalc = true;
					}
				}
			}
			const char *pText = NULL;
			if(HintRequiresStringCheck)
			{
				if(UIElement.Size() == 3)
				{
					pText = GetTextLambda();
					if(str_comp(UIElement.Get(0)->m_Text.c_str(), pText) != 0)
					{
						NeedsRecalc = true;
					}
				}
			}
			if(NeedsRecalc)
			{
				if(UIElement.Size() > 0)
				{
					UI()->ResetUIElement(UIElement);
				}

				vec4 RealColor = Color;
				for(int i = 0; i < 3; ++i)
				{
					Color.a = RealColor.a;
					if(i == 0)
						Color.a *= ButtonColorMulActive();
					else if(i == 1)
						Color.a *= ButtonColorMulHot();
					else if(i == 2)
						Color.a *= ButtonColorMulDefault();
					Graphics()->SetColor(Color);

					CUIElement::SUIElementRect NewRect;
					NewRect.m_UIRectQuadContainer = RenderTools()->CreateRoundRectQuadContainer(pRect->x, pRect->y, pRect->w, pRect->h, r, Corners);

					NewRect.m_X = pRect->x;
					NewRect.m_Y = pRect->y;
					NewRect.m_Width = pRect->w;
					NewRect.m_Height = pRect->h;

					if(i == 0)
					{
						if(pText == NULL)
							pText = GetTextLambda();
						NewRect.m_Text = pText;
						UI()->DoLabel(NewRect, &Text, pText, Text.h * ms_FontmodHeight, 0, -1, AlignVertically);
					}
					UIElement.Add(NewRect);
				}
				Graphics()->SetColor(1, 1, 1, 1);
			}
		}

		// render
		size_t Index = 2;
		if(UI()->ActiveItem() == pID)
			Index = 0;
		else if(UI()->HotItem() == pID)
			Index = 1;
		Graphics()->TextureClear();
		Graphics()->RenderQuadContainer(UIElement.Get(Index)->m_UIRectQuadContainer, -1);
		STextRenderColor ColorText(TextRender()->DefaultTextColor());
		STextRenderColor ColorTextOutline(TextRender()->DefaultTextOutlineColor());
		if(UIElement.Get(0)->m_UITextContainer != -1)
			TextRender()->RenderTextContainer(UIElement.Get(0)->m_UITextContainer, &ColorText, &ColorTextOutline);

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
	int UiDoListboxEnd(float *pScrollValue, bool *pItemActivated, bool *pListBoxActive = 0);

	//static void demolist_listdir_callback(const char *name, int is_dir, void *user);
	//static void demolist_list_callback(const CUIRect *rect, int index, void *user);

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

protected:
	sorted_array<SCustomEntities> m_EntitiesList;
	sorted_array<SCustomGame> m_GameList;
	sorted_array<SCustomEmoticon> m_EmoticonList;
	sorted_array<SCustomParticle> m_ParticlesList;

	static void LoadEntities(struct SCustomEntities *pEntitiesItem, void *pUser);
	static int EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser);

	static int GameScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser);

	void ClearCustomItems(int CurTab);

	int m_MenuPage;
	int m_GamePage;
	int m_Popup;
	int m_ActivePage;
	bool m_ShowStart;
	bool m_MenuActive;
	bool m_UseMouseButtons;
	vec2 m_MousePos;
	bool m_MouseSlow;

	int64 m_LastInput;

	// images
	struct CMenuImage
	{
		char m_aName[64];
		IGraphics::CTextureHandle m_OrgTexture;
		IGraphics::CTextureHandle m_GreyTexture;
	};
	array<CMenuImage> m_lMenuImages;

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
	static float ms_FontmodHeight;

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

	//
	bool m_EscapePressed;
	bool m_EnterPressed;
	bool m_DeletePressed;

	// for map download popup
	int64 m_DownloadLastCheckTime;
	int m_DownloadLastCheckSize;
	float m_DownloadSpeed;

	// for call vote
	int m_CallvoteSelectedOption;
	int m_CallvoteSelectedPlayer;
	char m_aCallvoteReason[VOTE_REASON_LENGTH];
	char m_aFilterString[25];

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
		char m_aFilename[128];
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
			return ((m_TimelineMarkers.m_aNumTimelineMarkers[0] << 24) & 0xFF000000) | ((m_TimelineMarkers.m_aNumTimelineMarkers[1] << 16) & 0xFF0000) |
			       ((m_TimelineMarkers.m_aNumTimelineMarkers[2] << 8) & 0xFF00) | (m_TimelineMarkers.m_aNumTimelineMarkers[3] & 0xFF);
		}

		int Length() const
		{
			return ((m_Info.m_aLength[0] << 24) & 0xFF000000) | ((m_Info.m_aLength[1] << 16) & 0xFF0000) |
			       ((m_Info.m_aLength[2] << 8) & 0xFF00) | (m_Info.m_aLength[3] & 0xFF);
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
				return str_comp_nocase(Left.m_aFilename, Right.m_aFilename) < 0;
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

	//sorted_array<CDemoItem> m_lDemos;
	char m_aCurrentDemoFolder[256];
	char m_aCurrentDemoFile[64];
	int m_DemolistSelectedIndex;
	bool m_DemolistSelectedIsDir;
	int m_DemolistStorageType;
	int m_Speed = 4;

	void DemolistOnUpdate(bool Reset);
	//void DemolistPopulate();
	static int DemolistFetchCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser);

	// friends
	struct CFriendItem
	{
		const CFriendInfo *m_pFriendInfo;
		int m_NumFound;

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

	sorted_array<CFriendItem> m_lFriends;
	int m_FriendlistSelectedIndex;

	void FriendlistOnUpdate();

	// found in menus.cpp
	int Render();
	//void render_background();
	//void render_loading(float percent);
	int RenderMenubar(CUIRect r);
	void RenderNews(CUIRect MainView);

	// found in menus_demo.cpp
	static bool DemoFilterChat(const void *pData, int Size, void *pUser);
	bool FetchHeader(CDemoItem &Item);
	void FetchAllHeaders();
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

	void UseMouseButtons(bool Use) { m_UseMouseButtons = Use; }

	static CMenusKeyBinder m_Binder;

	CMenus();

	void RenderLoading();
	void RenderUpdating(const char *pCaption, int current = 0, int total = 0);

	bool IsActive() const { return m_MenuActive; }
	void KillServer();

	virtual void OnInit();

	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnReset();
	virtual void OnRender();
	virtual bool OnInput(IInput::CEvent Event);
	virtual bool OnMouseMove(float x, float y);

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
		SETTINGS_HUD,
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
	int DoButton_CheckBox_DontCare(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	sorted_array<CDemoItem> m_lDemos;
	void DemolistPopulate();
	bool m_Dummy;

	const char *GetCurrentDemoFolder() const { return m_aCurrentDemoFolder; }

	// Ghost
	struct CGhostItem
	{
		char m_aFilename[256];
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

	sorted_array<CGhostItem> m_lGhosts;

	void GhostlistPopulate();
	CGhostItem *GetOwnGhost();
	void UpdateOwnGhost(CGhostItem Item);
	void DeleteGhostItem(int Index);

	void setPopup(int Popup) { m_Popup = Popup; }
	int GetCurPopup() { return m_Popup; }
	bool CanDisplayWarning();

	void PopupWarning(const char *pTopic, const char *pBody, const char *pButton, int64 Duration);

	int64 m_PopupWarningLastTime;
	int64 m_PopupWarningDuration;

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
		POPUP_PURE,
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

		// demo player states
		DEMOPLAYER_NONE = 0,
		DEMOPLAYER_SLICE_SAVE,
	};

private:
	static int GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	void SetMenuPage(int NewPage);
	bool HandleListInputs(const CUIRect &View, float &ScrollValue, float ScrollAmount, int *pScrollOffset, float ElemHeight, int &SelectedIndex, int NumElems);

	// found in menus_ingame.cpp
	void RenderInGameNetwork(CUIRect MainView);
	void RenderGhost(CUIRect MainView);

	// found in menus_settings.cpp
	void RenderSettingsDDNet(CUIRect MainView);
	void RenderSettingsHUD(CUIRect MainView);
	ColorHSLA RenderHSLColorPicker(const CUIRect *pRect, unsigned int *pColor, bool Alpha);
	ColorHSLA RenderHSLScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha = false);

	CServerProcess m_ServerProcess;
};
#endif
