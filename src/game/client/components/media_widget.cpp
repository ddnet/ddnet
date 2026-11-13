#include "media_widget.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/media_control.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

static constexpr ColorRGBA CARD_COLOR(0.0f, 0.0f, 0.0f, 0.5f);
static constexpr float CARD_ROUNDING = 7.5f;
static constexpr float DISABLED_ALPHA = 0.35f;

static void FormatPosition(int64_t Ms, char *pBuf, int BufSize)
{
	const int64_t TotalSeconds = std::max<int64_t>(Ms, 0) / 1000;
	const int64_t Hours = TotalSeconds / 3600;
	const int Minutes = (TotalSeconds / 60) % 60;
	const int Seconds = TotalSeconds % 60;
	if(Hours > 0)
		str_format(pBuf, BufSize, "%d:%02d:%02d", (int)Hours, Minutes, Seconds);
	else
		str_format(pBuf, BufSize, "%d:%02d", Minutes, Seconds);
}

void CMediaWidget::ConMediaPlayPause(IConsole::IResult *pResult, void *pUserData)
{
	CMediaWidget *pSelf = static_cast<CMediaWidget *>(pUserData);
	if(pSelf->m_pMediaControl != nullptr)
		pSelf->m_pMediaControl->PlayPause();
}

void CMediaWidget::ConMediaNext(IConsole::IResult *pResult, void *pUserData)
{
	CMediaWidget *pSelf = static_cast<CMediaWidget *>(pUserData);
	if(pSelf->m_pMediaControl != nullptr)
		pSelf->m_pMediaControl->Next();
}

void CMediaWidget::ConMediaPrevious(IConsole::IResult *pResult, void *pUserData)
{
	CMediaWidget *pSelf = static_cast<CMediaWidget *>(pUserData);
	if(pSelf->m_pMediaControl != nullptr)
		pSelf->m_pMediaControl->Previous();
}

void CMediaWidget::OnConsoleInit()
{
	Console()->Register("media_play_pause", "", CFGFLAG_CLIENT, ConMediaPlayPause, this, "Play or pause the media that is playing on the system");
	Console()->Register("media_next", "", CFGFLAG_CLIENT, ConMediaNext, this, "Skip to the next track of the media that is playing on the system");
	Console()->Register("media_prev", "", CFGFLAG_CLIENT, ConMediaPrevious, this, "Skip to the previous track of the media that is playing on the system");
}

void CMediaWidget::OnInit()
{
	m_pMediaControl = Kernel()->RequestInterface<IMediaControl>();
}

void CMediaWidget::UpdateArtwork(int Version)
{
	if(Version == m_ArtworkVersion)
		return;
	m_ArtworkVersion = Version;

	Graphics()->UnloadTexture(&m_ArtworkTexture);
	CImageInfo Image;
	if(Version != 0 && m_pMediaControl->CopyArtwork(Image))
	{
		m_ArtworkAspect = Image.m_Width / (float)Image.m_Height;
		m_ArtworkTexture = Graphics()->LoadTextureRawMove(Image, 0, "media artwork");
	}
}

void CMediaWidget::RenderArtwork(CUIRect Art)
{
	if(!m_ArtworkTexture.IsValid())
	{
		// placeholder
		Art.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f), IGraphics::CORNER_ALL, 5.0f);
		CUIRect ArtIcon;
		Art.Margin(Art.h * 0.25f, &ArtIcon);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		TextRender()->TextColor(TextRender()->DefaultTextColor().WithMultipliedAlpha(0.8f));
		Ui()->DoLabel(&ArtIcon, FontIcon::MUSIC, ArtIcon.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		return;
	}

	// crop the artwork to a square, showing its center
	float CropX = 0.0f;
	float CropY = 0.0f;
	if(m_ArtworkAspect > 1.0f)
		CropX = (1.0f - 1.0f / m_ArtworkAspect) / 2.0f;
	else
		CropY = (1.0f - m_ArtworkAspect) / 2.0f;

	Graphics()->TextureSet(m_ArtworkTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetSubset(CropX, CropY, 1.0f - CropX, 1.0f - CropY);
	const IGraphics::CQuadItem QuadItem(Art.x, Art.y, Art.w, Art.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
}

bool CMediaWidget::DoIconButton(CButtonContainer *pButtonContainer, const char *pIcon, const CUIRect *pRect, bool Enabled, bool Interactive)
{
	const bool Clickable = Enabled && Interactive;
	const bool Clicked = Clickable && Ui()->DoButtonLogic(pButtonContainer, 0, pRect, BUTTONFLAG_LEFT) != 0;
	if(Clickable && (Ui()->HotItem() == pButtonContainer || Ui()->CheckActiveItem(pButtonContainer)))
	{
		pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Ui()->CheckActiveItem(pButtonContainer) ? 0.3f : 0.2f), IGraphics::CORNER_ALL, 4.0f);
	}

	CUIRect Label;
	pRect->HMargin(1.5f, &Label);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextColor(TextRender()->DefaultTextColor().WithMultipliedAlpha(Enabled ? 1.0f : DISABLED_ALPHA));
	Ui()->DoLabel(&Label, pIcon, Label.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Clicked;
}

void CMediaWidget::RenderProgress(CUIRect Progress, int64_t PositionMs, int64_t DurationMs)
{
	constexpr float FontSize = 6.5f;
	constexpr float BarHeight = 2.5f;
	constexpr float Gap = 4.0f;

	char aPosition[32];
	char aDuration[32];
	FormatPosition(PositionMs, aPosition, sizeof(aPosition));
	FormatPosition(DurationMs, aDuration, sizeof(aDuration));

	// reserve the same width for both times, so the bar does not change its size while playing
	const float TimeWidth = TextRender()->TextWidth(FontSize, DurationMs >= 3600 * 1000 ? "0:00:00" : "00:00");

	CUIRect PositionLabel, DurationLabel, Bar;
	Progress.VSplitLeft(TimeWidth, &PositionLabel, &Progress);
	Progress.VSplitLeft(Gap, nullptr, &Progress);
	Progress.VSplitRight(TimeWidth, &Progress, &DurationLabel);
	Progress.VSplitRight(Gap, &Bar, nullptr);

	TextRender()->TextColor(TextRender()->DefaultTextColor().WithMultipliedAlpha(0.7f));
	Ui()->DoLabel(&PositionLabel, aPosition, FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&DurationLabel, aDuration, FontSize, TEXTALIGN_MR);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	Bar.HMargin((Bar.h - BarHeight) / 2.0f, &Bar);
	Bar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.2f), IGraphics::CORNER_ALL, BarHeight / 2.0f);

	const float Fraction = std::clamp(PositionMs / (float)DurationMs, 0.0f, 1.0f);
	CUIRect Filled = Bar;
	Filled.w = std::max(Bar.w * Fraction, BarHeight);
	Filled.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f), IGraphics::CORNER_ALL, BarHeight / 2.0f);
}

void CMediaWidget::Render(CUIRect Card, bool Interactive)
{
	constexpr float Padding = 6.0f;
	constexpr float TitleFontSize = 10.0f;
	constexpr float ArtistFontSize = 7.5f;
	constexpr float ButtonWidth = 24.0f;
	constexpr float ButtonSpacing = 8.0f;

	const CMediaState State = m_pMediaControl->State();

	Card.Draw(CARD_COLOR, IGraphics::CORNER_ALL, CARD_ROUNDING);
	Card.Margin(Padding, &Card);

	CUIRect Art, Body;
	Card.VSplitLeft(Card.h, &Art, &Body);
	Body.VSplitLeft(Padding + 2.0f, nullptr, &Body);
	UpdateArtwork(State.m_Available ? State.m_ArtworkVersion : 0);
	RenderArtwork(Art);

	CUIRect Title, Artist, Progress, Buttons;
	Body.HSplitTop(13.0f, &Title, &Body);
	Body.HSplitTop(10.0f, &Artist, &Body);
	Body.HSplitTop(2.0f, nullptr, &Body);
	Body.HSplitTop(9.0f, &Progress, &Body);
	Body.HSplitTop(1.0f, nullptr, &Buttons);

	// title and artist
	const char *pTitle;
	char aSubtitle[256];
	if(!State.m_Available)
	{
		pTitle = Localize("Nothing playing");
		str_copy(aSubtitle, Localize("System media"));
	}
	else
	{
		pTitle = State.m_aTitle[0] != '\0' ? State.m_aTitle : Localize("Unknown title");
		if(State.m_aArtist[0] != '\0' && State.m_aSource[0] != '\0')
			str_format(aSubtitle, sizeof(aSubtitle), "%s · %s", State.m_aArtist, State.m_aSource);
		else
			str_copy(aSubtitle, State.m_aArtist[0] != '\0' ? State.m_aArtist : State.m_aSource);
	}

	SLabelProperties TitleProps;
	TitleProps.m_MaxWidth = Title.w;
	TitleProps.m_EllipsisAtEnd = true;
	TitleProps.m_EnableWidthCheck = false;
	Ui()->DoLabel(&Title, pTitle, TitleFontSize, TEXTALIGN_ML, TitleProps);

	SLabelProperties ArtistProps;
	ArtistProps.m_MaxWidth = Artist.w;
	ArtistProps.m_EllipsisAtEnd = true;
	ArtistProps.m_EnableWidthCheck = false;
	TextRender()->TextColor(TextRender()->DefaultTextColor().WithMultipliedAlpha(0.7f));
	Ui()->DoLabel(&Artist, aSubtitle, ArtistFontSize, TEXTALIGN_ML, ArtistProps);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// progress
	if(State.m_Available && State.m_PositionMs >= 0 && State.m_DurationMs > 0)
	{
		int64_t PositionMs = State.m_PositionMs;
		if(State.m_Playing)
		{
			// the position is only sampled occasionally, so advance it locally while playing
			PositionMs += std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - State.m_PositionTimestamp).count();
		}
		RenderProgress(Progress, std::clamp<int64_t>(PositionMs, 0, State.m_DurationMs), State.m_DurationMs);
	}

	// buttons, centered below the progress bar
	const float ButtonsWidth = 3 * ButtonWidth + 2 * ButtonSpacing;
	Buttons.VMargin((Buttons.w - ButtonsWidth) / 2.0f, &Buttons);
	CUIRect PreviousButton, PlayPauseButton, NextButton;
	Buttons.VSplitLeft(ButtonWidth, &PreviousButton, &Buttons);
	Buttons.VSplitLeft(ButtonSpacing, nullptr, &Buttons);
	Buttons.VSplitLeft(ButtonWidth, &PlayPauseButton, &Buttons);
	Buttons.VSplitLeft(ButtonSpacing, nullptr, &Buttons);
	Buttons.VSplitLeft(ButtonWidth, &NextButton, nullptr);

	if(DoIconButton(&m_PreviousButton, FontIcon::BACKWARD_STEP, &PreviousButton, State.m_Available && State.m_CanPrevious, Interactive))
	{
		m_pMediaControl->Previous();
	}
	if(DoIconButton(&m_PlayPauseButton, State.m_Playing ? FontIcon::PAUSE : FontIcon::PLAY, &PlayPauseButton, State.m_Available && State.m_CanPlayPause, Interactive))
	{
		m_pMediaControl->PlayPause();
	}
	if(DoIconButton(&m_NextButton, FontIcon::FORWARD_STEP, &NextButton, State.m_Available && State.m_CanNext, Interactive))
	{
		m_pMediaControl->Next();
	}
}
