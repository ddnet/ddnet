/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/component.h>

class CNamePlateRenderData
{
public:
	bool m_InGame;
	vec2 m_Position;
	ColorRGBA m_Color;
	float m_Alpha;
	bool m_ShowName;
	const char *m_pName;
	bool m_ShowFriendMark;
	bool m_ShowClientId;
	int m_ClientId;
	float m_FontSizeClientId;
	bool m_ClientIdNewLine;
	float m_FontSize;
	bool m_ShowClan;
	const char *m_pClan;
	float m_FontSizeClan;
	bool m_ShowDirection;
	bool m_DirLeft;
	bool m_DirJump;
	bool m_DirRight;
	float m_FontSizeDirection;
	bool m_ShowHookStrength;
	enum
	{
		NAMEPLATE_HOOKSTRENGTH_WEAK,
		NAMEPLATE_HOOKSTRENGTH_UNKNOWN,
		NAMEPLATE_HOOKSTRENGTH_STRONG
	} m_HookStrength;
	bool m_ShowHookStrengthId;
	int m_HookStrengthId;
	float m_FontSizeHookStrength;
};

class CNamePlatePart
{
protected:
	float m_Width = 0.0f;
	float m_Height = 0.0f;
	float m_PaddingX = 5.0f;
	float m_PaddingY = 5.0f;
	// Offset to rendered X and Y not effecting layout
	float m_OffsetX = 0.0f;
	float m_OffsetY = 0.0f;
	// Whether this part is a new line (doesn't do anything else)
	bool m_NewLine = false;
	// Whether this part is visible
	bool m_Visible = true;
	// Whether when not visible will still take up space
	bool m_ShiftOnInvis = false;

public:
	friend class CGameClient;
	virtual void Update(CGameClient &This, const CNamePlateRenderData &Data) {}
	virtual void Reset(CGameClient &This){};
	virtual void Render(CGameClient &This, float X, float Y){};
	float Width() const { return m_Width; }
	float Height() const { return m_Height; }
	float PaddingX() const { return m_PaddingX; }
	float PaddingY() const { return m_PaddingY; }
	float OffsetX() const { return m_OffsetX; }
	float OffsetY() const { return m_OffsetY; }
	bool NewLine() const { return m_NewLine; }
	bool Visible() const { return m_Visible; }
	bool ShiftOnInvis() const { return m_ShiftOnInvis; }
	virtual ~CNamePlatePart() = default;
};

using NamePlatePartsVector = std::vector<std::unique_ptr<CNamePlatePart>>;

class CNamePlate
{
private:
	bool m_Inited = false;
	NamePlatePartsVector m_vpParts;
	void RenderLine(CGameClient &This, const CNamePlateRenderData &Data,
		float X, float Y, float W, float H,
		NamePlatePartsVector::iterator Start, NamePlatePartsVector::iterator End);
	template<typename PartType, typename... ArgsType>
	void AddPart(CGameClient &This, ArgsType &&... Args);

public:
	friend class CGameClient;
	void Init(CGameClient &This);
	void Reset(CGameClient &This);
	void Render(CGameClient &This, const CNamePlateRenderData &Data);
};

class CNamePlates : public CComponent
{
private:
	CNamePlate m_aNamePlates[MAX_CLIENTS];
	void RenderNamePlate(CNamePlate &NamePlate, const CNamePlateRenderData &Data);

public:
	void RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha);
	void RenderNamePlatePreview(vec2 Position, int Dummy);
	void ResetNamePlates();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnWindowResize() override;
};

#endif
