#ifndef GAME_CLIENT_COMPONENTS_MEDIA_WIDGET_H
#define GAME_CLIENT_COMPONENTS_MEDIA_WIDGET_H

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

class IMediaControl;

/**
 * Card that shows the media playing on the system and buttons to control it.
 *
 * It is not rendered on its own, the scoreboard renders it while it is active.
 */
class CMediaWidget : public CComponent
{
	IMediaControl *m_pMediaControl = nullptr;

	CButtonContainer m_PreviousButton;
	CButtonContainer m_PlayPauseButton;
	CButtonContainer m_NextButton;

	IGraphics::CTextureHandle m_ArtworkTexture;
	int m_ArtworkVersion = 0;
	float m_ArtworkAspect = 1.0f;

	void UpdateArtwork(int Version);
	void RenderArtwork(CUIRect Art);

	bool DoIconButton(CButtonContainer *pButtonContainer, const char *pIcon, const CUIRect *pRect, bool Enabled, bool Interactive);
	void RenderProgress(CUIRect Progress, int64_t PositionMs, int64_t DurationMs);

	static void ConMediaPlayPause(IConsole::IResult *pResult, void *pUserData);
	static void ConMediaNext(IConsole::IResult *pResult, void *pUserData);
	static void ConMediaPrevious(IConsole::IResult *pResult, void *pUserData);

public:
	static constexpr float WIDTH = 200.0f;
	static constexpr float HEIGHT = 62.0f;

	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnInit() override;

	/**
	 * Renders the media card.
	 *
	 * @param Card Area of the card, should have the size `WIDTH` x `HEIGHT`.
	 * @param Interactive Whether the buttons react to the mouse.
	 */
	void Render(CUIRect Card, bool Interactive);
};

#endif // GAME_CLIENT_COMPONENTS_MEDIA_WIDGET_H
