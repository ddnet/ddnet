#ifndef GAME_CLIENT_COMPONENTS_MULTIVIEW_H
#define GAME_CLIENT_COMPONENTS_MULTIVIEW_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CMultiView : public CComponent
{
public:
	CMultiView();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnMessage(int MsgId, void *pRawMsg) override;

	void UpdateActivation();
	void UpdateView();
	void CheckReset();

	bool IsActivated() const { return m_Activated; }
	void SetActivated(bool Activated) { m_Activated = Activated; }
	bool ShowHud() const { return m_ShowHud; }
	int Team() const { return m_Team; }

	bool IsIdSet(int ClientId) const { return ClientId >= 0 && ClientId < MAX_CLIENTS && m_aId[ClientId]; }
	void SetId(int ClientId, bool Set);
	void ToggleId(int ClientId);
	void ClearIds();
	bool IsAnyIdSet() const;

	float PersonalZoom() const { return m_PersonalZoom; }
	void AddPersonalZoom(float Amount) { m_PersonalZoom += Amount; }
	void SetPersonalZoom(float Zoom) { m_PersonalZoom = Zoom; }

	// Full reset, restoring the camera zoom as well.
	void Reset();
	int FindFirstId() const;
	void ClearId(int ClientId);

private:
	bool Init(int Team);
	void ClearAllIds();
	float CalculateMultiplier(vec2 TargetPos);
	float CalculateZoom(vec2 MinPos, vec2 MaxPos, float Vel);

	int m_Team;
	float m_PersonalZoom;
	bool m_ShowHud;
	bool m_Activated;
	bool m_aId[MAX_CLIENTS];

	// runtime state while multi-view is active
	bool m_Solo;
	bool m_IsInit;
	bool m_Teleported;
	bool m_aVanish[MAX_CLIENTS];
	vec2 m_OldPos;
	int m_OldPersonalZoom;
	float m_SecondChance;
	float m_OldCameraDistance;
	float m_aLastFreeze[MAX_CLIENTS];
};

#endif
