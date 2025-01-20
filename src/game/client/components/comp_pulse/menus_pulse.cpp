#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "../binds.h"
#include "../countryflags.h"
#include "../menus.h"
#include "../skins.h"

#include <array>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace FontIcons;
using namespace std::chrono_literals;

const float FontSize = 14.0f;
const float Margin = 10.0f;
const float HeaderHeight = FontSize + 5.0f + Margin;

const float LineSize = 20.0f;
const float ColorPickerLineSize = 25.0f;
const float HeadlineFontSize = 20.0f;
const float HeadlineHeight = 30.0f;
const float MarginSmall = 5.0f;
const float MarginBetweenViews = 20.0f;

const float ColorPickerLabelSize = 13.0f;
const float ColorPickerLineSpacing = 5.0f;

void CMenus::RenderSettingsPulse(CUIRect MainView)
{
static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 120.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.y += ScrollOffset.y;



	CUIRect DebugGroup;

	MainView.y -= 10.0f;

	// Начало одной секции
	static SFoldableSection s_InDebugGroup;
	MainView.HSplitTop(Margin, nullptr, &DebugGroup);
	DoFoldableSection(&s_InDebugGroup, Localize("General"), FontSize, &DebugGroup, &MainView, 5.0f, [&]() -> int {
		DebugGroup.VMargin(Margin, &DebugGroup);
		DebugGroup.HMargin(Margin, &DebugGroup);

		// Начало любого говнокода в секции
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Key is pressed (ALT): %d", Input()->KeyIsPressed(KEY_LALT));
		Ui()->DoLabel(&DebugGroup, aBuf, FontSize, TEXTALIGN_TL);

		DebugGroup.HSplitTop(LineSize, nullptr, &DebugGroup);

		char aBuf2[64];
		str_format(aBuf2, sizeof(aBuf2), "Global time: %f", Client()->GlobalTime());
		Ui()->DoLabel(&DebugGroup, aBuf2, FontSize, TEXTALIGN_TL);
		// Конец любого говнокода в секции

		int TotalHeight = 40.0f; // Длинна секции вниз
		return TotalHeight + Margin;
	});
	s_ScrollRegion.AddRect(DebugGroup);
	// Конец одной секции
}