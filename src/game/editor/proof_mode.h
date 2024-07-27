#ifndef GAME_EDITOR_PROOF_MODE_H
#define GAME_EDITOR_PROOF_MODE_H

#include "component.h"

class CProofMode : public CEditorComponent
{
public:
	void OnInit(CEditor *pEditor) override;
	void OnReset() override;
	void OnMapLoad() override;
	void RenderScreenSizes();

	bool IsEnabled() const;
	bool IsModeMenu() const;
	bool IsModeIngame() const;
	void Toggle();
	void SetModeMenu();
	void SetModeIngame();

	enum EProofBorder
	{
		PROOF_BORDER_OFF,
		PROOF_BORDER_INGAME,
		PROOF_BORDER_MENU
	};
	EProofBorder m_ProofBorders;

	int m_CurrentMenuProofIndex;
	std::vector<vec2> m_vMenuBackgroundPositions;
	std::vector<const char *> m_vpMenuBackgroundPositionNames;
	std::vector<std::vector<int>> m_vMenuBackgroundCollisions;

	void SetMenuBackgroundPositionNames();
	void ResetMenuBackgroundPositions();
};

#endif
