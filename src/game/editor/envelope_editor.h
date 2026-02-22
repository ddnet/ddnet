/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef GAME_EDITOR_ENVELOPE_EDITOR_H
#define GAME_EDITOR_ENVELOPE_EDITOR_H

#include <game/client/ui.h>
#include <game/editor/component.h>
#include <game/editor/editor_trackers.h>
#include <game/editor/smooth_value.h>
#include <game/mapitems.h>

#include <memory>
#include <vector>

class CEnvelope;

class CEnvelopeEditor : public CEditorComponent
{
public:
	class CState
	{
	public:
		CSmoothValue m_ZoomX;
		CSmoothValue m_ZoomY;
		bool m_ResetZoom;
		vec2 m_Offset;
		int m_ActiveChannels;

		void Reset(CEditor *pEditor);
	};

	void OnReset() override;
	void Render(CUIRect View);

private:
	void RenderColorBar(CUIRect ColorBar, const std::shared_ptr<CEnvelope> &pEnvelope);

	void UpdateHotEnvelopePoint(const CUIRect &View, const CEnvelope *pEnvelope, int ActiveChannels);

	void ZoomAdaptOffsetX(float ZoomFactor, const CUIRect &View);
	void UpdateZoomEnvelopeX(const CUIRect &View);

	void ZoomAdaptOffsetY(float ZoomFactor, const CUIRect &View);
	void UpdateZoomEnvelopeY(const CUIRect &View);

	void ResetZoomEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int ActiveChannels);
	void RemoveTimeOffsetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope);

	float ScreenToEnvelopeX(const CUIRect &View, float x) const;
	float EnvelopeToScreenX(const CUIRect &View, float x) const;
	float ScreenToEnvelopeY(const CUIRect &View, float y) const;
	float EnvelopeToScreenY(const CUIRect &View, float y) const;
	float ScreenToEnvelopeDeltaX(const CUIRect &View, float DeltaX);
	float ScreenToEnvelopeDeltaY(const CUIRect &View, float DeltaY);

	const char m_RedoButtonId = 0;
	const char m_UndoButtonId = 0;
	const char m_NewSoundEnvelopeButtonId = 0;
	const char m_NewColorEnvelopeButtonId = 0;
	const char m_NewPositionEnvelopeButtonId = 0;
	const char m_DeleteButtonId = 0;
	const char m_MoveRightButtonId = 0;
	const char m_MoveLeftButtonId = 0;
	const char m_ZoomOutButtonId = 0;
	const char m_ResetZoomButtonId = 0;
	const char m_ZoomInButtonId = 0;
	const char m_EnvelopeSelectorId = 0;
	const char m_PrevEnvelopeButtonId = 0;
	const char m_NextEnvelopeButtonId = 0;
	const char m_aChannelButtonIds[CEnvPoint::MAX_CHANNELS] = {0};
	const char m_EnvelopeEditorId = 0;
	CLineInput m_NameInput;
	int m_EnvelopeEditorButtonUsed;
	EEnvelopeEditorOp m_Operation;
	std::vector<float> m_vAccurateDragValuesX;
	std::vector<float> m_vAccurateDragValuesY;
	vec2 m_MouseStart;
	vec2 m_ScaleFactor;
	vec2 m_Midpoint;
	std::vector<float> m_vInitialPositionsX;
	std::vector<float> m_vInitialPositionsY;

	class CPopupEnvelopePoint : public SPopupMenuId
	{
	public:
		CEnvelopeEditor *m_pEnvelopeEditor;
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		int m_aValues[4] = {0};
		CLineInputNumber m_ValueInput;
		CLineInputNumber m_TimeInput;
		float m_CurrentTime = 0.0f;
		float m_CurrentValue = 0.0f;
		const char m_ColorPickerButtonId = 0;
		const char m_DeleteButtonId = 0;
	};
	CPopupEnvelopePoint m_PopupEnvelopePoint;

	class CPopupEnvelopePointMulti : public SPopupMenuId
	{
	public:
		CEnvelopeEditor *m_pEnvelopeEditor;
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_ProjectOntoButtonId = 0;
	};
	CPopupEnvelopePointMulti m_PopupEnvelopePointMulti;

	class CPopupEnvelopePointCurveType : public SPopupMenuId
	{
	public:
		CEnvelopeEditor *m_pEnvelopeEditor;
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_aButtonIds[NUM_CURVETYPES - 1] = {0};
	};
	CPopupEnvelopePointCurveType m_PopupEnvelopePointCurveType;

	class CPopupEnvelopeCurveType : public SPopupMenuId
	{
	public:
		CEnvelopeEditor *m_pEnvelopeEditor;
		int m_SelectedPoint;
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_aButtonIds[NUM_CURVETYPES] = {0};
	};
	CPopupEnvelopeCurveType m_PopupEnvelopeCurveType;
};

#endif
