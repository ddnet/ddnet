#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/generated/protocol.h>

class CNamePlateRenderData
{
public:
	bool m_InGame;
	vec2 m_Position;
	ColorRGBA m_Color;
	bool m_ShowName;
	const char *m_pName;
	bool m_ShowFriendMark;
	bool m_ShowClientId;
	int m_ClientId;
	float m_FontSizeClientId;
	bool m_ClientIdSeperateLine;
	float m_FontSize;
	bool m_ShowClan;
	const char *m_pClan;
	float m_FontSizeClan;
	bool m_ShowDirection;
	bool m_DirLeft;
	bool m_DirJump;
	bool m_DirRight;
	float m_FontSizeDirection;
	bool m_ShowHookStrongWeak;
	enum
	{
		HOOKSTRONGWEAK_WEAK,
		HOOKSTRONGWEAK_UNKNOWN,
		HOOKSTRONGWEAK_STRONG
	} m_HookStrongWeak;
	bool m_ShowHookStrongWeakId;
	int m_HookStrongWeakId;
	float m_FontSizeHookStrongWeak;
};

class CNamePlate;

class CNamePlates : public CComponent
{
private:
	CNamePlate *m_pNamePlates;

public:
	void RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha);
	void RenderNamePlatePreview(vec2 Position, int Dummy);
	void ResetNamePlates();
	int Sizeof() const override { return sizeof(*this); }
	void OnWindowResize() override;
	void OnRender() override;
	CNamePlates();
	~CNamePlates();
};

#endif
