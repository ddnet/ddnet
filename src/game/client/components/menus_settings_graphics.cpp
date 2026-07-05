/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <array>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button;
	char aBuf[128];
	bool CheckSettings = false;

	static const int MAX_RESOLUTIONS = 256;
	static CVideoMode s_aModes[MAX_RESOLUTIONS];
	static int s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	static int s_GfxFsaaSamples = g_Config.m_GfxFsaaSamples;
	static bool s_GfxBackendChanged = false;
	static bool s_GfxGpuChanged = false;

	static int s_InitDisplayAllVideoModes = g_Config.m_GfxDisplayAllVideoModes;

	static bool s_WasInit = false;
	static bool s_ModesReload = false;
	if(!s_WasInit)
	{
		s_WasInit = true;

		Graphics()->AddWindowPropChangeListener([]() {
			s_ModesReload = true;
		});
	}

	if(s_ModesReload || g_Config.m_GfxDisplayAllVideoModes != s_InitDisplayAllVideoModes)
	{
		s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		s_ModesReload = false;
		s_InitDisplayAllVideoModes = g_Config.m_GfxDisplayAllVideoModes;
	}

	CUIRect ModeList, ModeLabel;
	MainView.VSplitLeft(350.0f, &MainView, &ModeList);
	ModeList.HSplitTop(24.0f, &ModeLabel, &ModeList);
	MainView.VSplitLeft(340.0f, &MainView, nullptr);

	// display mode list
	static CListBox s_ListBox;
	static const float sc_RowHeightResList = 22.0f;
	static const float sc_FontSizeResListHeader = 12.0f;
	static const float sc_FontSizeResList = 10.0f;

	{
		int G = std::gcd(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);
		str_format(aBuf, sizeof(aBuf), "%s: %dx%d @%dhz (%d:%d)",
			Localize("Current"),
			(int)(g_Config.m_GfxScreenWidth * Graphics()->ScreenHiDPIScale()),
			(int)(g_Config.m_GfxScreenHeight * Graphics()->ScreenHiDPIScale()),
			g_Config.m_GfxScreenRefreshRate,
			g_Config.m_GfxScreenWidth / G,
			g_Config.m_GfxScreenHeight / G);
		Ui()->DoLabel(&ModeLabel, aBuf, sc_FontSizeResListHeader, TEXTALIGN_MC);
	}

	int OldSelected = -1;
	s_ListBox.SetActive(!Ui()->IsPopupOpen());
	s_ListBox.DoStart(sc_RowHeightResList, s_NumNodes, 1, 3, OldSelected, &ModeList);

	for(int i = 0; i < s_NumNodes; ++i)
	{
		if(g_Config.m_GfxScreenWidth == s_aModes[i].m_WindowWidth &&
			g_Config.m_GfxScreenHeight == s_aModes[i].m_WindowHeight &&
			g_Config.m_GfxScreenRefreshRate == s_aModes[i].m_RefreshRate)
		{
			OldSelected = i;
		}

		const CListboxItem Item = s_ListBox.DoNextItem(&s_aModes[i], OldSelected == i);
		if(!Item.m_Visible)
			continue;

		int G = std::gcd(s_aModes[i].m_WindowWidth, s_aModes[i].m_WindowHeight);
		str_format(aBuf, sizeof(aBuf), " %dx%d @%dhz (%d:%d)", s_aModes[i].m_CanvasWidth, s_aModes[i].m_CanvasHeight, s_aModes[i].m_RefreshRate, s_aModes[i].m_WindowWidth / G, s_aModes[i].m_WindowHeight / G);
		Ui()->DoLabel(&Item.m_Rect, aBuf, sc_FontSizeResList, TEXTALIGN_ML);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		g_Config.m_GfxScreenWidth = s_aModes[NewSelected].m_WindowWidth;
		g_Config.m_GfxScreenHeight = s_aModes[NewSelected].m_WindowHeight;
		g_Config.m_GfxScreenRefreshRate = s_aModes[NewSelected].m_RefreshRate;
		Graphics()->ResizeToScreen();
	}

	// switches
	CUIRect WindowModeDropDown;
	MainView.HSplitTop(20.0f, &WindowModeDropDown, &MainView);

	const char *apWindowModes[] = {Localize("Windowed"), Localize("Windowed borderless"), Localize("Windowed fullscreen"), Localize("Desktop fullscreen"), Localize("Fullscreen")};
	static const int s_NumWindowMode = std::size(apWindowModes);

	const int OldWindowMode = (g_Config.m_GfxFullscreen ? (g_Config.m_GfxFullscreen == 1 ? 4 : (g_Config.m_GfxFullscreen == 2 ? 3 : 2)) : (g_Config.m_GfxBorderless ? 1 : 0));

	static CUi::SDropDownState s_WindowModeDropDownState;
	static CScrollRegion s_WindowModeDropDownScrollRegion;
	s_WindowModeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_WindowModeDropDownScrollRegion;
	const int NewWindowMode = Ui()->DoDropDown(&WindowModeDropDown, OldWindowMode, apWindowModes, s_NumWindowMode, s_WindowModeDropDownState);
	if(OldWindowMode != NewWindowMode)
	{
		if(NewWindowMode == 0)
			Graphics()->SetWindowParams(0, false);
		else if(NewWindowMode == 1)
			Graphics()->SetWindowParams(0, true);
		else if(NewWindowMode == 2)
			Graphics()->SetWindowParams(3, false);
		else if(NewWindowMode == 3)
			Graphics()->SetWindowParams(2, false);
		else if(NewWindowMode == 4)
			Graphics()->SetWindowParams(1, false);
	}

	if(Graphics()->GetNumScreens() > 1)
	{
		CUIRect ScreenDropDown;
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &ScreenDropDown, &MainView);

		const int NumScreens = Graphics()->GetNumScreens();
		static std::vector<std::string> s_vScreenNames;
		static std::vector<const char *> s_vpScreenNames;
		s_vScreenNames.resize(NumScreens);
		s_vpScreenNames.resize(NumScreens);

		for(int i = 0; i < NumScreens; ++i)
		{
			str_format(aBuf, sizeof(aBuf), "%s %d: %s", Localize("Screen"), i, Graphics()->GetScreenName(i));
			s_vScreenNames[i] = aBuf;
			s_vpScreenNames[i] = s_vScreenNames[i].c_str();
		}

		static CUi::SDropDownState s_ScreenDropDownState;
		static CScrollRegion s_ScreenDropDownScrollRegion;
		s_ScreenDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ScreenDropDownScrollRegion;
		const int NewScreen = Ui()->DoDropDown(&ScreenDropDown, g_Config.m_GfxScreen, s_vpScreenNames.data(), s_vpScreenNames.size(), s_ScreenDropDownState);
		if(NewScreen != g_Config.m_GfxScreen)
			Graphics()->SwitchWindowScreen(NewScreen, true);
	}

	MainView.HSplitTop(2.0f, nullptr, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("V-Sync"), Localize("may cause delay"));
	if(DoButton_CheckBox(&g_Config.m_GfxVsync, aBuf, g_Config.m_GfxVsync, &Button))
	{
		Graphics()->SetVSync(!g_Config.m_GfxVsync);
	}

	bool MultiSamplingChanged = false;
	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("FSAA samples"), Localize("may cause delay"));
	int GfxFsaaSamplesMouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxFsaaSamples, aBuf, g_Config.m_GfxFsaaSamples, &Button);
	int CurFSAA = g_Config.m_GfxFsaaSamples == 0 ? 1 : g_Config.m_GfxFsaaSamples;
	if(GfxFsaaSamplesMouseButton == 1) // inc
	{
		g_Config.m_GfxFsaaSamples = std::pow(2, (int)std::log2(CurFSAA) + 1);
		if(g_Config.m_GfxFsaaSamples > 64)
			g_Config.m_GfxFsaaSamples = 0;
		MultiSamplingChanged = true;
	}
	else if(GfxFsaaSamplesMouseButton == 2) // dec
	{
		if(CurFSAA == 1)
			g_Config.m_GfxFsaaSamples = 64;
		else if(CurFSAA == 2)
			g_Config.m_GfxFsaaSamples = 0;
		else
			g_Config.m_GfxFsaaSamples = std::pow(2, (int)std::log2(CurFSAA) - 1);
		MultiSamplingChanged = true;
	}

	uint32_t MultiSamplingCountBackend = 0;
	if(MultiSamplingChanged)
	{
		if(Graphics()->SetMultiSampling(g_Config.m_GfxFsaaSamples, MultiSamplingCountBackend))
		{
			// try again with 0 if mouse click was increasing multi sampling
			// else just accept the current value as is
			if((uint32_t)g_Config.m_GfxFsaaSamples > MultiSamplingCountBackend && GfxFsaaSamplesMouseButton == 1)
				Graphics()->SetMultiSampling(0, MultiSamplingCountBackend);
			g_Config.m_GfxFsaaSamples = (int)MultiSamplingCountBackend;
		}
		else
		{
			CheckSettings = true;
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighDetail, Localize("High Detail"), g_Config.m_GfxHighDetail, &Button))
		g_Config.m_GfxHighDetail ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_GfxHighDetail, &Button, Localize("Allows maps to render with more detail"));

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClShowfps, Localize("Show FPS"), g_Config.m_ClShowfps, &Button))
		g_Config.m_ClShowfps ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClShowfps, &Button, Localize("Renders your frame rate in the top right"));

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_copy(aBuf, " ");
	str_append(aBuf, Localize("Hz", "Hertz"));
	Ui()->DoScrollbarOption(&g_Config.m_GfxRefreshRate, &g_Config.m_GfxRefreshRate, &Button, Localize("Refresh Rate"), 10, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE, aBuf);

	MainView.HSplitTop(2.0f, nullptr, &MainView);
	static CButtonContainer s_UiColorResetId;
	DoLine_ColorPicker(&s_UiColorResetId, 25.0f, 13.0f, 2.0f, &MainView, Localize("UI Color"), &g_Config.m_UiColor, color_cast<ColorRGBA>(ColorHSLA(0xE4A046AFU, true)), false, nullptr, true);

	// Backend list
	struct SMenuBackendInfo
	{
		int m_Major = 0;
		int m_Minor = 0;
		int m_Patch = 0;
		const char *m_pBackendName = "";
		bool m_Found = false;
	};
	std::array<std::array<SMenuBackendInfo, EGraphicsDriverAgeType::GRAPHICS_DRIVER_AGE_TYPE_COUNT>, EBackendType::BACKEND_TYPE_COUNT> aaSupportedBackends{};
	uint32_t FoundBackendCount = 0;
	for(uint32_t i = 0; i < BACKEND_TYPE_COUNT; ++i)
	{
		if(EBackendType(i) == BACKEND_TYPE_AUTO)
			continue;
		for(uint32_t n = 0; n < GRAPHICS_DRIVER_AGE_TYPE_COUNT; ++n)
		{
			auto &Info = aaSupportedBackends[i][n];
			if(Graphics()->GetDriverVersion(EGraphicsDriverAgeType(n), Info.m_Major, Info.m_Minor, Info.m_Patch, Info.m_pBackendName, EBackendType(i)))
			{
				// don't count blocked opengl drivers
				if(EBackendType(i) != BACKEND_TYPE_OPENGL || EGraphicsDriverAgeType(n) == GRAPHICS_DRIVER_AGE_TYPE_LEGACY || g_Config.m_GfxDriverIsBlocked == 0)
				{
					Info.m_Found = true;
					++FoundBackendCount;
				}
			}
		}
	}

	if(FoundBackendCount > 1)
	{
		CUIRect Text, BackendDropDown;
		MainView.HSplitTop(10.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Text, &MainView);
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &BackendDropDown, &MainView);
		Ui()->DoLabel(&Text, Localize("Renderer"), 16.0f, TEXTALIGN_MC);

		static std::vector<std::string> s_vBackendIdNames;
		static std::vector<const char *> s_vpBackendIdNamesCStr;
		static std::vector<SMenuBackendInfo> s_vBackendInfos;

		size_t BackendCount = FoundBackendCount + 1;
		s_vBackendIdNames.resize(BackendCount);
		s_vpBackendIdNamesCStr.resize(BackendCount);
		s_vBackendInfos.resize(BackendCount);

		char aTmpBackendName[256];

		auto IsInfoDefault = [](const SMenuBackendInfo &CheckInfo) {
			return str_comp_nocase(CheckInfo.m_pBackendName, DefaultConfig::GfxBackend) == 0 && CheckInfo.m_Major == DefaultConfig::GfxGLMajor && CheckInfo.m_Minor == DefaultConfig::GfxGLMinor && CheckInfo.m_Patch == DefaultConfig::GfxGLPatch;
		};

		int OldSelectedBackend = -1;
		uint32_t CurCounter = 0;
		for(uint32_t i = 0; i < BACKEND_TYPE_COUNT; ++i)
		{
			for(uint32_t n = 0; n < GRAPHICS_DRIVER_AGE_TYPE_COUNT; ++n)
			{
				auto &Info = aaSupportedBackends[i][n];
				if(Info.m_Found)
				{
					bool IsDefault = IsInfoDefault(Info);
					str_format(aTmpBackendName, sizeof(aTmpBackendName), "%s (%d.%d.%d)%s%s", Info.m_pBackendName, Info.m_Major, Info.m_Minor, Info.m_Patch, IsDefault ? " - " : "", IsDefault ? Localize("default") : "");
					s_vBackendIdNames[CurCounter] = aTmpBackendName;
					s_vpBackendIdNamesCStr[CurCounter] = s_vBackendIdNames[CurCounter].c_str();
					if(str_comp_nocase(Info.m_pBackendName, g_Config.m_GfxBackend) == 0 && g_Config.m_GfxGLMajor == Info.m_Major && g_Config.m_GfxGLMinor == Info.m_Minor && g_Config.m_GfxGLPatch == Info.m_Patch)
					{
						OldSelectedBackend = CurCounter;
					}

					s_vBackendInfos[CurCounter] = Info;
					++CurCounter;
				}
			}
		}

		if(OldSelectedBackend != -1)
		{
			// no custom selected
			BackendCount -= 1;
		}
		else
		{
			// custom selected one
			str_format(aTmpBackendName, sizeof(aTmpBackendName), "%s (%s %d.%d.%d)", Localize("custom"), g_Config.m_GfxBackend, g_Config.m_GfxGLMajor, g_Config.m_GfxGLMinor, g_Config.m_GfxGLPatch);
			s_vBackendIdNames[CurCounter] = aTmpBackendName;
			s_vpBackendIdNamesCStr[CurCounter] = s_vBackendIdNames[CurCounter].c_str();
			OldSelectedBackend = CurCounter;

			s_vBackendInfos[CurCounter].m_pBackendName = "custom";
			s_vBackendInfos[CurCounter].m_Major = g_Config.m_GfxGLMajor;
			s_vBackendInfos[CurCounter].m_Minor = g_Config.m_GfxGLMinor;
			s_vBackendInfos[CurCounter].m_Patch = g_Config.m_GfxGLPatch;
		}

		static int s_OldSelectedBackend = -1;
		if(s_OldSelectedBackend == -1)
			s_OldSelectedBackend = OldSelectedBackend;

		static CUi::SDropDownState s_BackendDropDownState;
		static CScrollRegion s_BackendDropDownScrollRegion;
		s_BackendDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_BackendDropDownScrollRegion;
		const int NewBackend = Ui()->DoDropDown(&BackendDropDown, OldSelectedBackend, s_vpBackendIdNamesCStr.data(), BackendCount, s_BackendDropDownState);
		if(OldSelectedBackend != NewBackend)
		{
			str_copy(g_Config.m_GfxBackend, s_vBackendInfos[NewBackend].m_pBackendName);
			g_Config.m_GfxGLMajor = s_vBackendInfos[NewBackend].m_Major;
			g_Config.m_GfxGLMinor = s_vBackendInfos[NewBackend].m_Minor;
			g_Config.m_GfxGLPatch = s_vBackendInfos[NewBackend].m_Patch;

			CheckSettings = true;
			s_GfxBackendChanged = s_OldSelectedBackend != NewBackend;
		}
	}

	// GPU list
	const auto &GpuList = Graphics()->GetGpus();
	if(GpuList.m_vGpus.size() > 1)
	{
		CUIRect Text, GpuDropDown;
		MainView.HSplitTop(10.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Text, &MainView);
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &GpuDropDown, &MainView);
		Ui()->DoLabel(&Text, Localize("Graphics card"), 16.0f, TEXTALIGN_MC);

		static std::vector<const char *> s_vpGpuIdNames;

		size_t GpuCount = GpuList.m_vGpus.size() + 1;
		s_vpGpuIdNames.resize(GpuCount);

		char aCurDeviceName[256 + 4];

		int OldSelectedGpu = -1;
		for(size_t i = 0; i < GpuCount; ++i)
		{
			if(i == 0)
			{
				str_format(aCurDeviceName, sizeof(aCurDeviceName), "%s (%s)", Localize("auto"), GpuList.m_AutoGpu.m_aName);
				s_vpGpuIdNames[i] = aCurDeviceName;
				if(str_comp("auto", g_Config.m_GfxGpuName) == 0)
				{
					OldSelectedGpu = 0;
				}
			}
			else
			{
				s_vpGpuIdNames[i] = GpuList.m_vGpus[i - 1].m_aName;
				if(str_comp(GpuList.m_vGpus[i - 1].m_aName, g_Config.m_GfxGpuName) == 0)
				{
					OldSelectedGpu = i;
				}
			}
		}

		static int s_OldSelectedGpu = -1;
		if(s_OldSelectedGpu == -1)
			s_OldSelectedGpu = OldSelectedGpu;

		static CUi::SDropDownState s_GpuDropDownState;
		static CScrollRegion s_GpuDropDownScrollRegion;
		s_GpuDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_GpuDropDownScrollRegion;
		const int NewGpu = Ui()->DoDropDown(&GpuDropDown, OldSelectedGpu, s_vpGpuIdNames.data(), GpuCount, s_GpuDropDownState);
		if(OldSelectedGpu != NewGpu)
		{
			if(NewGpu == 0)
				str_copy(g_Config.m_GfxGpuName, "auto");
			else
				str_copy(g_Config.m_GfxGpuName, GpuList.m_vGpus[NewGpu - 1].m_aName);
			CheckSettings = true;
			s_GfxGpuChanged = NewGpu != s_OldSelectedGpu;
		}
	}

	// check if the new settings require a restart
	if(CheckSettings)
	{
		m_NeedRestartGraphics = !(s_GfxFsaaSamples == g_Config.m_GfxFsaaSamples &&
					  !s_GfxBackendChanged &&
					  !s_GfxGpuChanged);
	}
}
