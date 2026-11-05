#ifndef GAME_CLIENT_COMPONENTS_UI_TUTORIAL_H
#define GAME_CLIENT_COMPONENTS_UI_TUTORIAL_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <array>
#include <optional>

class CUiTutorial : public CComponent
{
public:
	enum class ETarget
	{
		NONE = -1,
		START_PLAY,
		BROWSER_TABS,
		BROWSER_LIST,
		BROWSER_TOOLBOX,
		BROWSER_TOOLBOX_TAB,
		BROWSER_SEARCH,
		BROWSER_CONNECT,
		NUM,
	};

	enum class ETutorial
	{
		SERVER_BROWSER = 0,
		NUM,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;

	void Start(ETutorial Tutorial);
	bool IsActive() const { return m_ActiveTutorial.has_value(); }

	// Stops the tutorial because the player finished or closed it
	void Stop();

	// Guard so actions like joining a server should not be triggered by interacting with the highlighted area
	bool IsMenuOverlayVisible() const { return m_Visible; }

	// Registers the rect of an UI element for the current frame for highlighting
	void SetTargetRect(ETarget Target, const CUIRect &Rect);

	void RenderMenuOverlay();

private:
	enum class EBoxSide
	{
		AUTO,
		BELOW,
		ABOVE,
		RIGHT,
		LEFT,
	};

	struct SStep
	{
		const char *m_pTitle;
		const char *m_pText;
		// Highlighted UI element registered by another component
		ETarget m_Target = ETarget::NONE;
		// Element that is highlighted first before the highlight expands to m_Target,
		// e.g. the tab that was selected to show the page that the step explains
		ETarget m_IntroTarget = ETarget::NONE;
		// Hardcoded highlighted area in normalized screen coordinates (0 to 1), used if m_Target is NONE
		CUIRect m_Area = {0.0f, 0.0f, 0.0f, 0.0f};
		// If set, the step does not have a next button. Instead, the highlighted area can be
		// interacted with and the step is completed when this target is shown
		ETarget m_CompletedByTarget = ETarget::NONE;
		// If set, the highlighted area can be interacted with
		bool m_AllowInteraction = false;
		// Called once when the step is entered, e.g. to open the page that the step explains
		void (*m_pfnOnEnter)(CUiTutorial *pThis) = nullptr;
		// Called once when switching from this step to another step, e.g. to undo changes the user made
		void (*m_pfnOnLeave)(CUiTutorial *pThis) = nullptr;
		// Side of the highlighted area where the info box is preferably shown, e.g. to not cover other elements
		EBoxSide m_InfoBoxSide = EBoxSide::AUTO;
	};

	struct STutorialInfo
	{
		const char *m_pName;
		const SStep *m_pSteps;
		int m_NumSteps;
		// Whether the tutorial is stopped when connecting to a server
		bool m_OfflineOnly;
	};

	static const STutorialInfo &TutorialInfo(ETutorial Tutorial);
	const SStep &CurrentStep() const;
	int NumSteps() const;

	// Stops the tutorial without marking it as finished, unlike Stop.
	void Reset();
	void SetStep(int Step);
	void DeactivateMenuTextInput();
	void NextStep();
	void PreviousStep();
	bool CanGoBack() const;
	bool SelectTopServer();

	std::optional<CUIRect> FindTargetRect(ETarget Target) const;
	std::optional<CUIRect> FindHighlightRect(const SStep &Step, const CUIRect &Screen) const;
	void RenderOverlay();
	void RenderDim(const CUIRect &Screen, const std::optional<CUIRect> &Highlight, float Alpha) const;
	void RenderInfoBox(const CUIRect &Box, const SStep &Step);
	float InfoBoxHeight(const SStep &Step, float Width) const;
	CUIRect PlaceInfoBox(const CUIRect &Screen, const std::optional<CUIRect> &Highlight, const SStep &Step) const;

	static void ConUiTutorial(IConsole::IResult *pResult, void *pUserData);
	static void ConUiTutorialStop(IConsole::IResult *pResult, void *pUserData);

	std::optional<ETutorial> m_ActiveTutorial;
	int m_Step = 0;
	float m_StepStartTime = 0.0f;
	bool m_Visible = false; // whether the overlay was rendered in the last frame, so input is intercepted
	bool m_Interactive = false; // whether the visible step is completed by using the highlighted area
	bool m_PassThrough = false; // whether the mouse is over a highlighted area that accepts input

	// Highlight rect of the previous step to animate the transition to the new one.
	std::optional<CUIRect> m_LastHighlight;
	std::optional<CUIRect> m_TransitionFrom;
	// Position of the info box of the previous step to move it to the new position.
	std::optional<vec2> m_LastBoxPosition;
	std::optional<vec2> m_BoxTransitionFrom;

	std::array<std::optional<CUIRect>, (size_t)ETarget::NUM> m_aTargetRects;

	char m_BlockerId; // hot item that prevents UI elements below the overlay from being activated
	CButtonContainer m_CloseButton;
	CButtonContainer m_BackButton;
	CButtonContainer m_NextButton;
};

#endif
