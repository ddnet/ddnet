#ifndef GAME_EDITOR_PROOF_MODE_H
#define GAME_EDITOR_PROOF_MODE_H

#include "component.h"

#include <base/vmath.h>

#include <vector>

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
	int CurrentMenuProofIndex() const;
	void SetCurrentMenuProofIndex(int MenuProofIndex);
	const std::vector<vec2> &MenuBackgroundPositions() const;
	vec2 CurrentMenuBackgroundPosition() const;
	const char *MenuBackgroundPositionName(int MenuProofIndex) const;
	const std::vector<int> &MenuBackgroundCollisions(int MenuProofIndex) const;
	void InitMenuBackgroundPositions();

private:
	enum class EProofBorder
	{
		OFF,
		INGAME,
		MENU,
	};
	EProofBorder m_ProofBorders;

	int m_CurrentMenuProofIndex;
	std::vector<vec2> m_vMenuBackgroundPositions;
	std::vector<const char *> m_vpMenuBackgroundPositionNames;
	std::vector<std::vector<int>> m_vvMenuBackgroundCollisions;

	void InitMenuBackgroundPositionNames();
};

#endif
