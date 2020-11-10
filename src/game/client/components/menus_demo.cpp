/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <base/hash.h>
#include <base/math.h>

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/console.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/localization.h>

#include <game/client/ui.h>

#include <game/generated/client_data.h>

#include "maplayers.h"
#include "menus.h"

int CMenus::DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	RenderTools()->DrawUIRect(pRect, ColorRGBA(1, 1, 1, (Checked ? 0.10f : 0.5f) * ButtonColorMul(pID)), CUI::CORNER_ALL, 5.0f);
	UI()->DoLabel(pRect, pText, 14.0f, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_Sprite(const void *pID, int ImageID, int SpriteID, int Checked, const CUIRect *pRect, int Corners)
{
	RenderTools()->DrawUIRect(pRect, ColorRGBA(1.0f, 1.0f, 1.0f, (Checked ? 0.10f : 0.5f) * ButtonColorMul(pID)), Corners, 5.0f);
	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	if(!Checked)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return UI()->DoButtonLogic(pID, "", Checked, pRect);
}

bool CMenus::DemoFilterChat(const void *pData, int Size, void *pUser)
{
	bool DoFilterChat = *(bool *)pUser;
	if(!DoFilterChat)
	{
		return false;
	}

	CUnpacker Unpacker;
	Unpacker.Reset(pData, Size);

	int Msg = Unpacker.GetInt();
	int Sys = Msg & 1;
	Msg >>= 1;

	return !Unpacker.Error() && !Sys && Msg == NETMSGTYPE_SV_CHAT;
}

void CMenus::RenderDemoPlayer(CUIRect MainView)
{
	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();

	const float SeekBarHeight = 15.0f;
	const float ButtonbarHeight = 20.0f;
	const float NameBarHeight = 20.0f;
	const float Margins = 5.0f;
	float TotalHeight;
	static int64 LastSpeedChange = 0;

	// render popups
	if(m_DemoPlayerState == DEMOPLAYER_SLICE_SAVE)
	{
		CUIRect Screen = *UI()->Screen();
		CUIRect Box, Part, Part2;
		Box = Screen;
		Box.VMargin(150.0f / UI()->Scale(), &Box);
		Box.HMargin(150.0f / UI()->Scale(), &Box);

		// render the box
		RenderTools()->DrawUIRect(&Box, ColorRGBA(0, 0, 0, 0.5f), CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		UI()->DoLabelScaled(&Part, Localize("Select a name"), 24.f, 0);
		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f / UI()->Scale(), &Part);
		UI()->DoLabelScaled(&Part, m_aDemoPlayerPopupHint, 24.f, 0);

		CUIRect Label, TextBox, Ok, Abort;

		Box.HSplitBottom(20.f, &Box, &Part);
		Box.HSplitBottom(24.f, &Box, &Part);
		Part.VMargin(80.0f, &Part);

		Part.VSplitMid(&Abort, &Ok);

		Ok.VMargin(20.0f, &Ok);
		Abort.VMargin(20.0f, &Abort);

		static int s_RemoveChat = 0;

		static int s_ButtonAbort = 0;
		if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
			m_DemoPlayerState = DEMOPLAYER_NONE;

		static int s_ButtonOk = 0;
		if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
		{
			if(str_comp(m_lDemos[m_DemolistSelectedIndex].m_aFilename, m_aCurrentDemoFile) == 0)
				str_copy(m_aDemoPlayerPopupHint, Localize("Please use a different name"), sizeof(m_aDemoPlayerPopupHint));
			else
			{
				if(!str_endswith(m_aCurrentDemoFile, ".demo"))
					str_append(m_aCurrentDemoFile, ".demo", sizeof(m_aCurrentDemoFile));

				char aPath[512];
				str_format(aPath, sizeof(aPath), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);

				IOHANDLE DemoFile = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
				const char *pStr = Localize("File already exists, do you want to overwrite it?");
				if(DemoFile && str_comp(m_aDemoPlayerPopupHint, pStr) != 0)
				{
					io_close(DemoFile);
					str_copy(m_aDemoPlayerPopupHint, pStr, sizeof(m_aDemoPlayerPopupHint));
				}
				else
				{
					m_DemoPlayerState = DEMOPLAYER_NONE;
					Client()->DemoSlice(aPath, CMenus::DemoFilterChat, &s_RemoveChat);
					DemolistPopulate();
					DemolistOnUpdate(false);
				}
			}
		}

		Box.HSplitBottom(60.f, &Box, &Part);
		Box.HSplitBottom(60.f, &Box, &Part2);
		Box.HSplitBottom(24.f, &Box, &Part2);
		Box.HSplitBottom(24.f, &Box, &Part);

		Part2.VSplitLeft(60.0f, 0, &Label);
		if(DoButton_CheckBox(&s_RemoveChat, Localize("Remove chat"), s_RemoveChat, &Label))
		{
			s_RemoveChat ^= 1;
		}

		Part.VSplitLeft(60.0f, 0, &Label);
		Label.VSplitLeft(120.0f, 0, &TextBox);
		TextBox.VSplitLeft(20.0f, 0, &TextBox);
		TextBox.VSplitRight(60.0f, &TextBox, 0);
		UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
		static float Offset = 0.0f;
		if(DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset))
		{
			m_aDemoPlayerPopupHint[0] = '\0';
		}
	}

	// handle keyboard shortcuts independent of active menu
	if(m_pClient->m_pGameConsole->IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE && g_Config.m_ClDemoKeyboardShortcuts)
	{
		// increase/decrease speed
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) || Input()->KeyPress(KEY_UP))
		{
			DemoPlayer()->SetSpeedIndex(+1);
			LastSpeedChange = time_get();
		}
		else if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) || Input()->KeyPress(KEY_DOWN))
		{
			DemoPlayer()->SetSpeedIndex(-1);
			LastSpeedChange = time_get();
		}

		// pause/unpause
		if(Input()->KeyPress(KEY_SPACE) || Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_K))
		{
			if(pInfo->m_Paused)
			{
				DemoPlayer()->Unpause();
			}
			else
			{
				DemoPlayer()->Pause();
			}
		}

		// seek backward/forward 10/5 seconds
		if(Input()->KeyPress(KEY_J))
		{
			DemoPlayer()->SeekTime(-10.0f);
		}
		else if(Input()->KeyPress(KEY_L))
		{
			DemoPlayer()->SeekTime(10.0f);
		}
		else if(Input()->KeyPress(KEY_LEFT))
		{
			DemoPlayer()->SeekTime(-5.0f);
		}
		else if(Input()->KeyPress(KEY_RIGHT))
		{
			DemoPlayer()->SeekTime(5.0f);
		}

		// seek to 0-90%
		const int SeekPercentKeys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
		for(unsigned i = 0; i < sizeof(SeekPercentKeys) / sizeof(SeekPercentKeys[0]); i++)
		{
			if(Input()->KeyPress(SeekPercentKeys[i]))
			{
				DemoPlayer()->SeekPercent(i * 0.1f);
				break;
			}
		}

		// seek to the beginning/end
		if(Input()->KeyPress(KEY_HOME))
		{
			DemoPlayer()->SeekPercent(0.0f);
		}
		else if(Input()->KeyPress(KEY_END))
		{
			DemoPlayer()->SeekPercent(1.0f);
		}
	}

	TotalHeight = SeekBarHeight + ButtonbarHeight + NameBarHeight + Margins * 3;

	// render speed info
	if(g_Config.m_ClDemoShowSpeed && time_get() - LastSpeedChange < time_freq() * 1)
	{
		CUIRect Screen = *UI()->Screen();

		char aSpeedBuf[256];
		str_format(aSpeedBuf, sizeof(aSpeedBuf), "×%.2f", pInfo->m_Speed);
		TextRender()->Text(0, 120.0f, Screen.y + Screen.h - 120.0f - TotalHeight, 60.0f, aSpeedBuf, -1.0f);
	}

	if(!m_MenuActive)
		return;

	MainView.HSplitBottom(TotalHeight, 0, &MainView);
	MainView.VSplitLeft(50.0f, 0, &MainView);
	MainView.VSplitLeft(450.0f, &MainView, 0);

	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_T, 10.0f);

	MainView.Margin(5.0f, &MainView);

	CUIRect SeekBar, ButtonBar, NameBar;

	int CurrentTick = pInfo->m_CurrentTick - pInfo->m_FirstTick;
	int TotalTicks = pInfo->m_LastTick - pInfo->m_FirstTick;

	MainView.HSplitTop(SeekBarHeight, &SeekBar, &ButtonBar);
	ButtonBar.HSplitTop(Margins, 0, &ButtonBar);
	ButtonBar.HSplitBottom(NameBarHeight, &ButtonBar, &NameBar);
	NameBar.HSplitTop(4.0f, 0, &NameBar);

	// do seekbar
	{
		static int s_SeekBarID = 0;
		void *id = &s_SeekBarID;
		char aBuffer[128];

		// draw seek bar
		RenderTools()->DrawUIRect(&SeekBar, ColorRGBA(0, 0, 0, 0.5f), CUI::CORNER_ALL, 5.0f);

		// draw filled bar
		float Amount = CurrentTick / (float)TotalTicks;
		CUIRect FilledBar = SeekBar;
		FilledBar.w = 10.0f + (FilledBar.w - 10.0f) * Amount;
		RenderTools()->DrawUIRect(&FilledBar, ColorRGBA(1, 1, 1, 0.5f), CUI::CORNER_ALL, 5.0f);

		// draw markers
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			float Ratio = (pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw slice markers
		// begin
		if(g_Config.m_ClDemoSliceBegin != -1)
		{
			float Ratio = (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(10.0f + SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// end
		if(g_Config.m_ClDemoSliceEnd != -1)
		{
			float Ratio = (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(10.0f + SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw time
		char aCurrentTime[32];
		str_time((int64)CurrentTick / SERVER_TICK_SPEED * 100, TIME_HOURS, aCurrentTime, sizeof(aCurrentTime));
		char aTotalTime[32];
		str_time((int64)TotalTicks / SERVER_TICK_SPEED * 100, TIME_HOURS, aTotalTime, sizeof(aTotalTime));
		str_format(aBuffer, sizeof(aBuffer), "%s / %s", aCurrentTime, aTotalTime);
		UI()->DoLabel(&SeekBar, aBuffer, SeekBar.h * 0.70f, 0);

		// do the logic
		int Inside = UI()->MouseInside(&SeekBar);

		if(UI()->ActiveItem() == id)
		{
			if(!UI()->MouseButton(0))
				UI()->SetActiveItem(0);
			else
			{
				static float PrevAmount = 0.0f;
				float Amount = (UI()->MouseX() - SeekBar.x) / SeekBar.w;

				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				{
					Amount = PrevAmount + (Amount - PrevAmount) * 0.05f;

					if(Amount > 0.0f && Amount < 1.0f && absolute(PrevAmount - Amount) >= 0.0001f)
					{
						//PrevAmount = Amount;
						m_pClient->m_SuppressEvents = true;
						DemoPlayer()->SeekPercent(Amount);
						m_pClient->m_SuppressEvents = false;
						m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
						m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
					}
				}
				else
				{
					if(Amount > 0.0f && Amount < 1.0f && absolute(PrevAmount - Amount) >= 0.001f)
					{
						PrevAmount = Amount;
						m_pClient->m_SuppressEvents = true;
						DemoPlayer()->SeekPercent(Amount);
						m_pClient->m_SuppressEvents = false;
						m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
						m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
					}
				}
			}
		}
		else if(UI()->HotItem() == id)
		{
			if(UI()->MouseButton(0))
				UI()->SetActiveItem(id);
		}

		if(Inside)
			UI()->SetHotItem(id);
	}

	if(CurrentTick == TotalTicks)
	{
		DemoPlayer()->Pause();
		DemoPlayer()->SeekPercent(0.0f);
	}

	bool IncreaseDemoSpeed = false, DecreaseDemoSpeed = false;

	// do buttons
	CUIRect Button;

	// combined play and pause button
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_PlayPauseButton = 0;
	if(DoButton_Sprite(&s_PlayPauseButton, IMAGE_DEMOBUTTONS, pInfo->m_Paused ? SPRITE_DEMOBUTTON_PLAY : SPRITE_DEMOBUTTON_PAUSE, false, &Button, CUI::CORNER_ALL))
	{
		if(pInfo->m_Paused)
		{
			DemoPlayer()->Unpause();
		}
		else
		{
			DemoPlayer()->Pause();
		}
	}

	// stop button

	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_ResetButton = 0;
	if(DoButton_Sprite(&s_ResetButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_STOP, false, &Button, CUI::CORNER_ALL))
	{
		DemoPlayer()->Pause();
		DemoPlayer()->SeekPercent(0.0f);
	}

	// slowdown
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_SlowDownButton = 0;
	if(DoButton_Sprite(&s_SlowDownButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_SLOWER, 0, &Button, CUI::CORNER_ALL))
		DecreaseDemoSpeed = true;

	// fastforward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_FastForwardButton = 0;
	if(DoButton_Sprite(&s_FastForwardButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_FASTER, 0, &Button, CUI::CORNER_ALL))
		IncreaseDemoSpeed = true;

	// speed meter
	ButtonBar.VSplitLeft(Margins * 3, 0, &ButtonBar);
	char aBuffer[64];
	str_format(aBuffer, sizeof(aBuffer), "×%g", pInfo->m_Speed);
	UI()->DoLabel(&ButtonBar, aBuffer, Button.h * 0.7f, -1);

	// slice begin button
	ButtonBar.VSplitLeft(Margins * 10, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_SliceBeginButton = 0;
	if(DoButton_Sprite(&s_SliceBeginButton, IMAGE_DEMOBUTTONS2, SPRITE_DEMOBUTTON_SLICE_BEGIN, 0, &Button, CUI::CORNER_ALL))
		Client()->DemoSliceBegin();

	// slice end button
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_SliceEndButton = 0;
	if(DoButton_Sprite(&s_SliceEndButton, IMAGE_DEMOBUTTONS2, SPRITE_DEMOBUTTON_SLICE_END, 0, &Button, CUI::CORNER_ALL))
		Client()->DemoSliceEnd();

	// slice save button
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static int s_SliceSaveButton = 0;
	if(DoButton_Sprite(&s_SliceSaveButton, IMAGE_FILEICONS, SPRITE_FILE_DEMO2, 0, &Button, CUI::CORNER_ALL))
	{
		str_copy(m_aCurrentDemoFile, m_lDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aCurrentDemoFile));
		m_aDemoPlayerPopupHint[0] = '\0';
		m_DemoPlayerState = DEMOPLAYER_SLICE_SAVE;
	}

	// close button
	ButtonBar.VSplitRight(ButtonbarHeight * 3, &ButtonBar, &Button);
	static int s_ExitButton = 0;
	if(DoButton_DemoPlayer(&s_ExitButton, Localize("Close"), 0, &Button) || (Input()->KeyPress(KEY_C) && m_pClient->m_pGameConsole->IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE))
	{
		Client()->Disconnect();
		DemolistOnUpdate(false);
	}

	// toggle keyboard shortcuts button
	ButtonBar.VSplitRight(Margins * 3, &ButtonBar, 0);
	ButtonBar.VSplitRight(ButtonbarHeight, &ButtonBar, &Button);
	static int s_KeyboardShortcutsButton = 0;
	int Sprite = g_Config.m_ClDemoKeyboardShortcuts ? SPRITE_DEMOBUTTON_SHORTCUTS_ENABLED : SPRITE_DEMOBUTTON_SHORTCUTS_DISABLED;
	if(DoButton_Sprite(&s_KeyboardShortcutsButton, IMAGE_DEMOBUTTONS2, Sprite, 0, &Button, CUI::CORNER_ALL))
	{
		g_Config.m_ClDemoKeyboardShortcuts ^= 1;
	}

	// demo name
	char aDemoName[64] = {0};
	DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), Localize("Demofile: %s"), aDemoName);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, NameBar.x, NameBar.y + (NameBar.h - (Button.h * 0.5f)) / 2.f, Button.h * 0.5f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = MainView.w;
	TextRender()->TextEx(&Cursor, aBuf, -1);

	if(IncreaseDemoSpeed)
	{
		DemoPlayer()->SetSpeedIndex(+1);
		LastSpeedChange = time_get();
	}
	else if(DecreaseDemoSpeed)
	{
		DemoPlayer()->SetSpeedIndex(-1);
		LastSpeedChange = time_get();
	}
}

static CUIRect gs_ListBoxOriginalView;
static CUIRect gs_ListBoxView;
static float gs_ListBoxRowHeight;
static int gs_ListBoxItemIndex;
static int gs_ListBoxSelectedIndex;
static int gs_ListBoxNewSelected;
static int gs_ListBoxDoneEvents;
static int gs_ListBoxNumItems;
static int gs_ListBoxItemsPerRow;
static float gs_ListBoxScrollValue;
static bool gs_ListBoxItemActivated;
static bool gs_ListBoxClicked;

void CMenus::UiDoListboxStart(const void *pID, const CUIRect *pRect, float RowHeight, const char *pTitle, const char *pBottomText, int NumItems,
	int ItemsPerRow, int SelectedIndex, float ScrollValue, bool LogicOnly)
{
	CUIRect Scroll, Row;
	CUIRect View = *pRect;
	CUIRect Header, Footer;

	if(!LogicOnly)
	{
		// draw header
		View.HSplitTop(ms_ListheaderHeight, &Header, &View);
		RenderTools()->DrawUIRect(&Header, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_T, 5.0f);
		UI()->DoLabel(&Header, pTitle, Header.h * ms_FontmodHeight, 0);

		// draw footers
		View.HSplitBottom(ms_ListheaderHeight, &View, &Footer);
		RenderTools()->DrawUIRect(&Footer, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_B, 5.0f);
		Footer.VSplitLeft(10.0f, 0, &Footer);
		UI()->DoLabel(&Footer, pBottomText, Header.h * ms_FontmodHeight, 0);

		// background
		RenderTools()->DrawUIRect(&View, ColorRGBA(0, 0, 0, 0.15f), 0, 0);
	}

	// prepare the scroll
	View.VSplitRight(15, &View, &Scroll);

	// setup the variables
	gs_ListBoxOriginalView = View;
	gs_ListBoxSelectedIndex = SelectedIndex;
	gs_ListBoxNewSelected = SelectedIndex;
	gs_ListBoxItemIndex = 0;
	gs_ListBoxRowHeight = RowHeight;
	gs_ListBoxNumItems = NumItems;
	gs_ListBoxItemsPerRow = ItemsPerRow;
	gs_ListBoxDoneEvents = 0;
	gs_ListBoxScrollValue = ScrollValue;
	gs_ListBoxItemActivated = false;
	gs_ListBoxClicked = false;

	// do the scrollbar
	View.HSplitTop(gs_ListBoxRowHeight, &Row, 0);

	int NumViewable = (int)(gs_ListBoxOriginalView.h / Row.h) + 1;
	int Num = (NumItems + gs_ListBoxItemsPerRow - 1) / gs_ListBoxItemsPerRow - NumViewable + 1;
	if(Num <= 0)
	{
		Num = 0;
	}
	else if(Num == 1)
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && UI()->MouseInside(&View))
			gs_ListBoxScrollValue -= 0.1f;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && UI()->MouseInside(&View))
			gs_ListBoxScrollValue += 0.1f;

		if(gs_ListBoxScrollValue < 0.0f)
			gs_ListBoxScrollValue = 0.0f;
		if(gs_ListBoxScrollValue > 1.0f)
			gs_ListBoxScrollValue = 1.0f;
	}
	else
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && UI()->MouseInside(&View))
			gs_ListBoxScrollValue -= 3.0f / Num;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && UI()->MouseInside(&View))
			gs_ListBoxScrollValue += 3.0f / Num;

		if(gs_ListBoxScrollValue < 0.0f)
			gs_ListBoxScrollValue = 0.0f;
		if(gs_ListBoxScrollValue > 1.0f)
			gs_ListBoxScrollValue = 1.0f;
	}

	Scroll.HMargin(5.0f, &Scroll);
	gs_ListBoxScrollValue = DoScrollbarV(pID, &Scroll, gs_ListBoxScrollValue);

	// the list
	gs_ListBoxView = gs_ListBoxOriginalView;
	gs_ListBoxView.VMargin(5.0f, &gs_ListBoxView);
	UI()->ClipEnable(&gs_ListBoxView);
	gs_ListBoxView.y -= gs_ListBoxScrollValue * Num * Row.h;
}

CMenus::CListboxItem CMenus::UiDoListboxNextRow()
{
	static CUIRect s_RowView;
	CListboxItem Item = {0};
	if(gs_ListBoxItemIndex % gs_ListBoxItemsPerRow == 0)
		gs_ListBoxView.HSplitTop(gs_ListBoxRowHeight /*-2.0f*/, &s_RowView, &gs_ListBoxView);

	s_RowView.VSplitLeft(s_RowView.w / (gs_ListBoxItemsPerRow - gs_ListBoxItemIndex % gs_ListBoxItemsPerRow) / (UI()->Scale()), &Item.m_Rect, &s_RowView);

	Item.m_Visible = 1;
	//item.rect = row;

	Item.m_HitRect = Item.m_Rect;

	//CUIRect select_hit_box = item.rect;

	if(gs_ListBoxSelectedIndex == gs_ListBoxItemIndex)
		Item.m_Selected = 1;

	// make sure that only those in view can be selected
	if(Item.m_Rect.y + Item.m_Rect.h > gs_ListBoxOriginalView.y)
	{
		if(Item.m_HitRect.y < Item.m_HitRect.y) // clip the selection
		{
			Item.m_HitRect.h -= gs_ListBoxOriginalView.y - Item.m_HitRect.y;
			Item.m_HitRect.y = gs_ListBoxOriginalView.y;
		}
	}
	else
		Item.m_Visible = 0;

	// check if we need to do more
	if(Item.m_Rect.y > gs_ListBoxOriginalView.y + gs_ListBoxOriginalView.h)
		Item.m_Visible = 0;

	gs_ListBoxItemIndex++;
	return Item;
}

CMenus::CListboxItem CMenus::UiDoListboxNextItem(const void *pId, bool Selected, bool KeyEvents, bool NoHoverEffects)
{
	int ThisItemIndex = gs_ListBoxItemIndex;
	if(Selected)
	{
		if(gs_ListBoxSelectedIndex == gs_ListBoxNewSelected)
			gs_ListBoxNewSelected = ThisItemIndex;
		gs_ListBoxSelectedIndex = ThisItemIndex;
	}

	CListboxItem Item = UiDoListboxNextRow();

	if(Item.m_Visible && UI()->DoButtonLogic(pId, "", gs_ListBoxSelectedIndex == gs_ListBoxItemIndex, &Item.m_HitRect))
	{
		gs_ListBoxClicked = true;
		gs_ListBoxNewSelected = ThisItemIndex;
	}

	// process input, regard selected index
	if(gs_ListBoxSelectedIndex == ThisItemIndex)
	{
		if(!gs_ListBoxDoneEvents)
		{
			gs_ListBoxDoneEvents = 1;

			if(m_EnterPressed || (UI()->ActiveItem() == pId && Input()->MouseDoubleClick()))
			{
				gs_ListBoxItemActivated = true;
				UI()->SetActiveItem(0);
			}
			else if(KeyEvents)
			{
				for(int i = 0; i < m_NumInputEvents; i++)
				{
					int NewIndex = -1;
					if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
					{
						if(m_aInputEvents[i].m_Key == KEY_DOWN)
							NewIndex = gs_ListBoxNewSelected + 1;
						else if(m_aInputEvents[i].m_Key == KEY_UP)
							NewIndex = gs_ListBoxNewSelected - 1;
						else if(m_aInputEvents[i].m_Key == KEY_PAGEUP)
							NewIndex = maximum(gs_ListBoxNewSelected - 20, 0);
						else if(m_aInputEvents[i].m_Key == KEY_PAGEDOWN)
							NewIndex = minimum(gs_ListBoxNewSelected + 20, gs_ListBoxNumItems - 1);
						else if(m_aInputEvents[i].m_Key == KEY_HOME)
							NewIndex = 0;
						else if(m_aInputEvents[i].m_Key == KEY_END)
							NewIndex = gs_ListBoxNumItems - 1;
					}
					if(NewIndex > -1 && NewIndex < gs_ListBoxNumItems)
					{
						// scroll
						float Offset = (NewIndex / gs_ListBoxItemsPerRow - gs_ListBoxNewSelected / gs_ListBoxItemsPerRow) * gs_ListBoxRowHeight;
						int Scroll = gs_ListBoxOriginalView.y > Item.m_Rect.y + Offset                                            ? -1 :
							     gs_ListBoxOriginalView.y + gs_ListBoxOriginalView.h < Item.m_Rect.y + Item.m_Rect.h + Offset ? 1 :
                                                                                                                                                            0;
						if(Scroll)
						{
							int NumViewable = (int)(gs_ListBoxOriginalView.h / gs_ListBoxRowHeight) + 1;
							int ScrollNum = (gs_ListBoxNumItems + gs_ListBoxItemsPerRow - 1) / gs_ListBoxItemsPerRow - NumViewable + 1;
							if(Scroll < 0)
							{
								int Num = (gs_ListBoxOriginalView.y - Item.m_Rect.y - Offset + gs_ListBoxRowHeight - 1.0f) / gs_ListBoxRowHeight;
								gs_ListBoxScrollValue -= (1.0f / ScrollNum) * Num;
							}
							else
							{
								int Num = (Item.m_Rect.y + Item.m_Rect.h + Offset - (gs_ListBoxOriginalView.y + gs_ListBoxOriginalView.h) + gs_ListBoxRowHeight - 1.0f) /
									  gs_ListBoxRowHeight;
								gs_ListBoxScrollValue += (1.0f / ScrollNum) * Num;
							}
							if(gs_ListBoxScrollValue < 0.0f)
								gs_ListBoxScrollValue = 0.0f;
							if(gs_ListBoxScrollValue > 1.0f)
								gs_ListBoxScrollValue = 1.0f;
						}

						gs_ListBoxNewSelected = NewIndex;
					}
				}
			}
		}

		//selected_index = i;
		CUIRect r = Item.m_Rect;
		r.Margin(1.5f, &r);
		RenderTools()->DrawUIRect(&r, ColorRGBA(1, 1, 1, 0.5f), CUI::CORNER_ALL, 4.0f);
	}
	else if(UI()->MouseInside(&Item.m_Rect) && !NoHoverEffects)
	{
		CUIRect r = Item.m_Rect;
		r.Margin(1.5f, &r);
		RenderTools()->DrawUIRect(&r, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 4.0f);
	}

	return Item;
}

int CMenus::UiDoListboxEnd(float *pScrollValue, bool *pItemActivated, bool *pListBoxActive)
{
	UI()->ClipDisable();
	if(pScrollValue)
		*pScrollValue = gs_ListBoxScrollValue;
	if(pItemActivated)
		*pItemActivated = gs_ListBoxItemActivated;
	if(pListBoxActive)
		*pListBoxActive = gs_ListBoxClicked;
	return gs_ListBoxNewSelected;
}

int CMenus::DemolistFetchCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	if(str_comp(pName, ".") == 0 || (str_comp(pName, "..") == 0 && str_comp(pSelf->m_aCurrentDemoFolder, "demos") == 0) || (!IsDir && !str_endswith(pName, ".demo")))
	{
		return 0;
	}

	CDemoItem Item;
	str_copy(Item.m_aFilename, pName, sizeof(Item.m_aFilename));
	if(IsDir)
	{
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pName);
		Item.m_InfosLoaded = false;
		Item.m_Valid = false;
		Item.m_Date = 0;
	}
	else
	{
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - 5);
		Item.m_InfosLoaded = false;
		Item.m_Date = Date;
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_StorageType = StorageType;
	pSelf->m_lDemos.add_unsorted(Item);

	return 0;
}

void CMenus::DemolistPopulate()
{
	m_lDemos.clear();
	if(!str_comp(m_aCurrentDemoFolder, "demos"))
		m_DemolistStorageType = IStorage::TYPE_ALL;
	Storage()->ListDirectoryInfo(m_DemolistStorageType, m_aCurrentDemoFolder, DemolistFetchCallback, this);

	if(g_Config.m_BrDemoFetchInfo)
		FetchAllHeaders();

	m_lDemos.sort_range();
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	if(Reset)
		g_Config.m_UiDemoSelected[0] = '\0';
	else
	{
		bool Found = false;
		int SelectedIndex = -1;
		// search for selected index
		for(sorted_array<CDemoItem>::range r = m_lDemos.all(); !r.empty(); r.pop_front())
		{
			SelectedIndex++;

			if(str_comp(g_Config.m_UiDemoSelected, r.front().m_aName) == 0)
			{
				Found = true;
				break;
			}
		}

		if(Found)
			m_DemolistSelectedIndex = SelectedIndex;
	}

	m_DemolistSelectedIndex = Reset                                      ? m_lDemos.size() > 0 ? 0 : -1 :
				  m_DemolistSelectedIndex >= m_lDemos.size() ? m_lDemos.size() - 1 :
                                                                               m_DemolistSelectedIndex;
	m_DemolistSelectedIsDir = m_DemolistSelectedIndex < 0 ? false : m_lDemos[m_DemolistSelectedIndex].m_IsDir;
}

bool CMenus::FetchHeader(CDemoItem &Item)
{
	if(!Item.m_InfosLoaded)
	{
		char aBuffer[512];
		str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_aCurrentDemoFolder, Item.m_aFilename);
		Item.m_Valid = DemoPlayer()->GetDemoInfo(Storage(), aBuffer, Item.m_StorageType, &Item.m_Info, &Item.m_TimelineMarkers, &Item.m_MapInfo);
		Item.m_InfosLoaded = true;
	}
	return Item.m_Valid;
}

void CMenus::FetchAllHeaders()
{
	for(sorted_array<CDemoItem>::range r = m_lDemos.all(); !r.empty(); r.pop_front())
	{
		FetchHeader(r.front());
	}
	m_lDemos.sort_range();
}

void CMenus::RenderDemoList(CUIRect MainView)
{
	static int s_Inited = 0;
	if(!s_Inited)
	{
		DemolistPopulate();
		DemolistOnUpdate(true);
		s_Inited = 1;
	}

	char aFooterLabel[128] = {0};
	if(m_DemolistSelectedIndex >= 0)
	{
		CDemoItem &Item = m_lDemos[m_DemolistSelectedIndex];
		if(str_comp(Item.m_aFilename, "..") == 0)
			str_copy(aFooterLabel, Localize("Parent Folder"), sizeof(aFooterLabel));
		else if(m_DemolistSelectedIsDir)
			str_copy(aFooterLabel, Localize("Folder"), sizeof(aFooterLabel));
		else if(!FetchHeader(Item))
			str_copy(aFooterLabel, Localize("Invalid Demo"), sizeof(aFooterLabel));
		else
			str_copy(aFooterLabel, Localize("Demo details"), sizeof(aFooterLabel));
	}

	// render background
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

#if defined(CONF_VIDEORECORDER)
	CUIRect RenderRect;
#endif
	CUIRect ButtonBar, RefreshRect, FetchRect, PlayRect, DeleteRect, RenameRect, LabelRect, ListBox;
	CUIRect ButtonBar2, DirectoryButton;

	MainView.HSplitBottom((ms_ButtonHeight + 5.0f) * 2.0f, &MainView, &ButtonBar2);
	ButtonBar2.HSplitTop(5.0f, 0, &ButtonBar2);
	ButtonBar2.HSplitTop(ms_ButtonHeight, &ButtonBar2, &ButtonBar);
	ButtonBar.HSplitTop(5.0f, 0, &ButtonBar);
	ButtonBar2.VSplitLeft(110.0f, &FetchRect, &ButtonBar2);
	ButtonBar2.VSplitLeft(10.0f, 0, &ButtonBar2);
	ButtonBar2.VSplitLeft(230.0f, &DirectoryButton, &ButtonBar2);
	ButtonBar2.VSplitLeft(10.0f, 0, &ButtonBar2);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &PlayRect);
	ButtonBar.VSplitLeft(110.0f, &RefreshRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &DeleteRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &RenameRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
#if defined(CONF_VIDEORECORDER)
	ButtonBar2.VSplitRight(110.0f, &ButtonBar2, &RenderRect);
	ButtonBar2.VSplitRight(10.0f, &ButtonBar2, 0);
#endif
	ButtonBar.VSplitLeft(110.0f, &LabelRect, &ButtonBar);
	MainView.HSplitBottom(140.0f, &ListBox, &MainView);

	// render demo info
	MainView.VMargin(5.0f, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, 0);
	RenderTools()->DrawUIRect(&MainView, ColorRGBA(0, 0, 0, 0.15f), CUI::CORNER_B, 4.0f);
	if(!m_DemolistSelectedIsDir && m_DemolistSelectedIndex >= 0 && m_lDemos[m_DemolistSelectedIndex].m_Valid)
	{
		CUIRect Left, Right, Labels;
		MainView.VMargin(20.0f, &MainView);
		MainView.HMargin(10.0f, &MainView);
		MainView.VSplitMid(&Labels, &MainView);

		// left side
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Created:"), 14.0f, -1);

		char aTimestamp[256];
		str_timestamp_ex(m_lDemos[m_DemolistSelectedIndex].m_Date, aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

		UI()->DoLabelScaled(&Right, aTimestamp, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Type:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aType, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Length:"), 14.0f, -1);
		int Length = m_lDemos[m_DemolistSelectedIndex].Length();
		char aBuf[64];
		str_time((int64)Length * 100, TIME_HOURS, aBuf, sizeof(aBuf));
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Version:"), 14.0f, -1);
		str_format(aBuf, sizeof(aBuf), "%d", m_lDemos[m_DemolistSelectedIndex].m_Info.m_Version);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Markers:"), 14.0f, -1);
		str_format(aBuf, sizeof(aBuf), "%d", m_lDemos[m_DemolistSelectedIndex].NumMarkers());
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);

		// right side
		Labels = MainView;
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Map:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapName, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Size:"), 14.0f, -1);
		unsigned Size = (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[0] << 24) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[1] << 16) |
				(m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[2] << 8) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[3]);
		if(Size > 1024 * 1024)
			str_format(aBuf, sizeof(aBuf), Localize("%.2f MiB"), float(Size) / (1024 * 1024));
		else
			str_format(aBuf, sizeof(aBuf), Localize("%.2f KiB"), float(Size) / 1024);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		if(m_lDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256 != SHA256_ZEROED)
		{
			UI()->DoLabelScaled(&Left, "SHA256:", 14.0f, -1);
			char aSha[SHA256_MAXSTRSIZE];
			sha256_str(m_lDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256, aSha, sizeof(aSha) / 2);
			UI()->DoLabelScaled(&Right, aSha, Right.w > 235 ? 14.0f : 11.0f, -1);
		}
		else
		{
			UI()->DoLabelScaled(&Left, Localize("Crc:"), 14.0f, -1);
			str_format(aBuf, sizeof(aBuf), "%08x", m_lDemos[m_DemolistSelectedIndex].m_MapInfo.m_Crc);
			UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		}
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);

		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Netversion:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aNetversion, 14.0f, -1);
	}

	// demo list

	CUIRect Headers;

	ListBox.HSplitTop(ms_ListheaderHeight, &Headers, &ListBox);

	struct CColumn
	{
		int m_ID;
		int m_Sort;
		CLocConstString m_Caption;
		int m_Direction;
		float m_Width;
		int m_Flags;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		COL_ICON = 0,
		COL_DEMONAME,
		COL_MARKERS,
		COL_LENGTH,
		COL_DATE,
	};

	static CColumn s_aCols[] = {
		{COL_ICON, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_DEMONAME, SORT_DEMONAME, "Demo", 0, 0.0f, 0, {0}, {0}},
		{COL_MARKERS, SORT_MARKERS, "Markers", 1, 75.0f, 0, {0}, {0}},
		{COL_LENGTH, SORT_LENGTH, "Length", 1, 75.0f, 0, {0}, {0}},
		{COL_DATE, SORT_DATE, "Date", 1, 160.0f, 1, {0}, {0}},
	};
	/* This is just for scripts/update_localization.py to work correctly. Don't remove!
		Localize("Demo");Localize("Markers");Localize("Length");Localize("Date");
	*/

	RenderTools()->DrawUIRect(&Headers, ColorRGBA(0.0f, 0, 0, 0.15f), 0, 0);

	int NumCols = sizeof(s_aCols) / sizeof(CColumn);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				//Cols[i].flags |= SPACER;
				Headers.VSplitLeft(2, &s_aCols[i].m_Spacer, &Headers);
			}
		}
	}

	for(int i = NumCols - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
			Headers.VSplitRight(2, &Headers, &s_aCols[i].m_Spacer);
		}
	}

	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == 0)
			s_aCols[i].m_Rect = Headers;
	}

	// do headers
	for(int i = 0; i < NumCols; i++)
	{
		if(DoButton_GridHeader(s_aCols[i].m_Caption, Localize(s_aCols[i].m_Caption), g_Config.m_BrDemoSort == s_aCols[i].m_Sort, &s_aCols[i].m_Rect))
		{
			if(s_aCols[i].m_Sort != -1)
			{
				if(g_Config.m_BrDemoSort == s_aCols[i].m_Sort)
					g_Config.m_BrDemoSortOrder ^= 1;
				else
					g_Config.m_BrDemoSortOrder = 0;
				g_Config.m_BrDemoSort = s_aCols[i].m_Sort;
			}

			// Don't rescan in order to keep fetched headers, just resort
			m_lDemos.sort_range();
			DemolistOnUpdate(false);
		}
	}

	// scrollbar
	CUIRect Scroll;
	ListBox.VSplitRight(15, &ListBox, &Scroll);

	int Num = (int)(ListBox.h / s_aCols[0].m_Rect.h) + 1;
	static int s_ScrollBar = 0;
	static float s_ScrollValue = 0;

	Scroll.HMargin(5.0f, &Scroll);
	s_ScrollValue = DoScrollbarV(&s_ScrollBar, &Scroll, s_ScrollValue);

	int ScrollNum = m_lDemos.size() - Num + 1;
	if(ScrollNum > 0)
	{
		if(m_ScrollOffset)
		{
			s_ScrollValue = (float)(m_ScrollOffset) / ScrollNum;
			m_ScrollOffset = 0;
		}
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && UI()->MouseInside(&ListBox))
			s_ScrollValue -= 3.0f / ScrollNum;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && UI()->MouseInside(&ListBox))
			s_ScrollValue += 3.0f / ScrollNum;
	}
	else
		ScrollNum = 0;

	if(m_DemolistSelectedIndex > -1)
	{
		for(int i = 0; i < m_NumInputEvents; i++)
		{
			int NewIndex = -1;
			if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
			{
				if(m_aInputEvents[i].m_Key == KEY_DOWN)
					NewIndex = m_DemolistSelectedIndex + 1;
				else if(m_aInputEvents[i].m_Key == KEY_UP)
					NewIndex = m_DemolistSelectedIndex - 1;
				else if(m_aInputEvents[i].m_Key == KEY_PAGEUP)
					NewIndex = maximum(m_DemolistSelectedIndex - 20, 0);
				else if(m_aInputEvents[i].m_Key == KEY_PAGEDOWN)
					NewIndex = minimum(m_DemolistSelectedIndex + 20, m_lDemos.size() - 1);
				else if(m_aInputEvents[i].m_Key == KEY_HOME)
					NewIndex = 0;
				else if(m_aInputEvents[i].m_Key == KEY_END)
					NewIndex = m_lDemos.size() - 1;
			}
			if(NewIndex > -1 && NewIndex < m_lDemos.size())
			{
				//scroll
				float IndexY = ListBox.y - s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h + NewIndex * s_aCols[0].m_Rect.h;
				int Scroll = ListBox.y > IndexY ? -1 : ListBox.y + ListBox.h < IndexY + s_aCols[0].m_Rect.h ? 1 :
                                                                                                                              0;
				if(Scroll)
				{
					if(Scroll < 0)
					{
						int NumScrolls = (ListBox.y - IndexY + s_aCols[0].m_Rect.h - 1.0f) / s_aCols[0].m_Rect.h;
						s_ScrollValue -= (1.0f / ScrollNum) * NumScrolls;
					}
					else
					{
						int NumScrolls = (IndexY + s_aCols[0].m_Rect.h - (ListBox.y + ListBox.h) + s_aCols[0].m_Rect.h - 1.0f) / s_aCols[0].m_Rect.h;
						s_ScrollValue += (1.0f / ScrollNum) * NumScrolls;
					}
				}

				m_DemolistSelectedIndex = NewIndex;

				str_copy(g_Config.m_UiDemoSelected, m_lDemos[NewIndex].m_aName, sizeof(g_Config.m_UiDemoSelected));
				DemolistOnUpdate(false);
			}
		}
	}

	if(s_ScrollValue < 0)
		s_ScrollValue = 0;
	if(s_ScrollValue > 1)
		s_ScrollValue = 1;

	// set clipping
	UI()->ClipEnable(&ListBox);

	CUIRect OriginalView = ListBox;
	ListBox.y -= s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h;

	int NewSelected = -1;
	int ItemIndex = -1;

	for(sorted_array<CDemoItem>::range r = m_lDemos.all(); !r.empty(); r.pop_front())
	{
		ItemIndex++;

		CUIRect Row;
		CUIRect SelectHitBox;

		ListBox.HSplitTop(ms_ListheaderHeight, &Row, &ListBox);
		SelectHitBox = Row;

		int Selected = ItemIndex == m_DemolistSelectedIndex;

		// make sure that only those in view can be selected
		if(Row.y + Row.h > OriginalView.y && Row.y < OriginalView.y + OriginalView.h)
		{
			if(Selected)
			{
				CUIRect r = Row;
				r.Margin(0.5f, &r);
				RenderTools()->DrawUIRect(&r, ColorRGBA(1, 1, 1, 0.5f), CUI::CORNER_ALL, 4.0f);
			}
			else if(UI()->MouseInside(&SelectHitBox))
			{
				CUIRect r = Row;
				r.Margin(0.5f, &r);
				RenderTools()->DrawUIRect(&r, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 4.0f);
			}

			// clip the selection
			if(SelectHitBox.y < OriginalView.y) // top
			{
				SelectHitBox.h -= OriginalView.y - SelectHitBox.y;
				SelectHitBox.y = OriginalView.y;
			}
			else if(SelectHitBox.y + SelectHitBox.h > OriginalView.y + OriginalView.h) // bottom
				SelectHitBox.h = OriginalView.y + OriginalView.h - SelectHitBox.y;

			if(UI()->DoButtonLogic(r.front().m_aName /* TODO: */, "", Selected, &SelectHitBox))
			{
				NewSelected = ItemIndex;
				str_copy(g_Config.m_UiDemoSelected, r.front().m_aName, sizeof(g_Config.m_UiDemoSelected));
				DemolistOnUpdate(false);
				m_DoubleClickIndex = NewSelected;
			}
		}
		else
		{
			// don't render invisible items
			continue;
		}

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Row.y;
			Button.h = Row.h;
			Button.w = s_aCols[c].m_Rect.w;

			int ID = s_aCols[c].m_ID;

			if(ID == COL_ICON)
			{
				DoButton_Icon(IMAGE_FILEICONS, r.front().m_IsDir ? SPRITE_FILE_FOLDER : SPRITE_FILE_DEMO1, &Button);
			}
			else if(ID == COL_DEMONAME)
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, Button.x, Button.y + (Button.h - 12.0f * UI()->Scale()) / 2.f, 12.0f * UI()->Scale(), TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;

				TextRender()->TextEx(&Cursor, r.front().m_aName, -1);
			}
			else if(ID == COL_MARKERS && !r.front().m_IsDir && r.front().m_InfosLoaded)
			{
				char aBuf[3];
				str_format(aBuf, sizeof(aBuf), "%d", r.front().NumMarkers());
				Button.VMargin(4.0f, &Button);
				UI()->DoLabelScaled(&Button, aBuf, 12.0f, 1);
			}
			else if(ID == COL_LENGTH && !r.front().m_IsDir && r.front().m_InfosLoaded)
			{
				int Length = r.front().Length();
				char aBuf[32];
				str_time((int64)Length * 100, TIME_HOURS, aBuf, sizeof(aBuf));
				Button.VMargin(4.0f, &Button);
				UI()->DoLabelScaled(&Button, aBuf, 12.0f, 1);
			}
			else if(ID == COL_DATE && !r.front().m_IsDir)
			{
				char aBuf[64];
				str_timestamp_ex(r.front().m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
				Button.VSplitRight(24.0f, &Button, 0);
				UI()->DoLabelScaled(&Button, aBuf, 12.0f, 1);
			}
		}
	}

	UI()->ClipDisable();

	bool Activated = false;

	if(m_EnterPressed || (Input()->MouseDoubleClick() && UI()->HotItem() == m_lDemos[m_DemolistSelectedIndex].m_aName))
	{
		UI()->SetActiveItem(0);
		Activated = true;
	}

	static int s_RefreshButton = 0;
	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshRect) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}

	if(DoButton_CheckBox(&g_Config.m_BrDemoFetchInfo, Localize("Fetch Info"), g_Config.m_BrDemoFetchInfo, &FetchRect))
	{
		g_Config.m_BrDemoFetchInfo ^= 1;
		if(g_Config.m_BrDemoFetchInfo)
			FetchAllHeaders();
	}

	static int s_PlayButton = 0;
	if(DoButton_Menu(&s_PlayButton, m_DemolistSelectedIsDir ? Localize("Open") : Localize("Play", "Demo browser"), 0, &PlayRect) || Activated || (Input()->KeyPress(KEY_P) && m_pClient->m_pGameConsole->IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE))
	{
		if(m_DemolistSelectedIndex >= 0)
		{
			if(m_DemolistSelectedIsDir) // folder
			{
				if(str_comp(m_lDemos[m_DemolistSelectedIndex].m_aFilename, "..") == 0) // parent folder
					fs_parent_dir(m_aCurrentDemoFolder);
				else // sub folder
				{
					char aTemp[256];
					str_copy(aTemp, m_aCurrentDemoFolder, sizeof(aTemp));
					str_format(m_aCurrentDemoFolder, sizeof(m_aCurrentDemoFolder), "%s/%s", aTemp, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					m_DemolistStorageType = m_lDemos[m_DemolistSelectedIndex].m_StorageType;
				}
				DemolistPopulate();
				DemolistOnUpdate(true);
			}
			else // file
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Play(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType);
				if(pError)
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
				else
				{
					UI()->SetActiveItem(0);
					return;
				}
			}
		}
	}

	if(DoButton_Menu(&DirectoryButton, Localize("Demos directory"), 0, &DirectoryButton))
	{
		char aBuf[MAX_PATH_LENGTH];
		char aBufFull[MAX_PATH_LENGTH + 7];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "demos", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("demos", IStorage::TYPE_SAVE);
		str_format(aBufFull, sizeof(aBufFull), "file://%s", aBuf);
		if(!open_link(aBufFull))
		{
			dbg_msg("menus", "couldn't open link");
		}
	}

	if(!m_DemolistSelectedIsDir)
	{
		static int s_DeleteButton = 0;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &DeleteRect) || m_DeletePressed || (Input()->KeyPress(KEY_D) && m_pClient->m_pGameConsole->IsClosed()))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_DELETE_DEMO;
				return;
			}
		}

		static int s_RenameButton = 0;
		if(DoButton_Menu(&s_RenameButton, Localize("Rename"), 0, &RenameRect))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_RENAME_DEMO;
				str_copy(m_aCurrentDemoFile, m_lDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aCurrentDemoFile));
				return;
			}
		}

#if defined(CONF_VIDEORECORDER)
		static int s_RenderButton = 0;
		if(DoButton_Menu(&s_RenderButton, Localize("Render"), 0, &RenderRect) || (Input()->KeyPress(KEY_R) && m_pClient->m_pGameConsole->IsClosed()))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_RENDER_DEMO;
				str_copy(m_aCurrentDemoFile, m_lDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aCurrentDemoFile));
				return;
			}
		}
#endif
	}

	UI()->DoLabelScaled(&LabelRect, aFooterLabel, 14.0f, -1);
}
