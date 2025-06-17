#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <engine/shared/protocol7.h>

#include <game/generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <memory>
#include <vector>

#include "nameplates.h"

static constexpr float DEFAULT_PADDING = 5.0f;

// Part Types

class CNamePlatePart
{
protected:
	vec2 m_Size = vec2(0.0f, 0.0f);
	vec2 m_Padding = vec2(DEFAULT_PADDING, DEFAULT_PADDING);
	bool m_NewLine = false; // Whether this part is a new line (doesn't do anything else)
	bool m_Visible = true; // Whether this part is visible
	bool m_ShiftOnInvis = false; // Whether when not visible will still take up space
	CNamePlatePart(CGameClient &This) {}

public:
	virtual void Update(CGameClient &This, const CNamePlateData &Data) {}
	virtual void Reset(CGameClient &This) {}
	virtual void Render(CGameClient &This, vec2 Pos) const {}
	vec2 Size() const { return m_Size; }
	vec2 Padding() const { return m_Padding; }
	bool NewLine() const { return m_NewLine; }
	bool Visible() const { return m_Visible; }
	bool ShiftOnInvis() const { return m_ShiftOnInvis; }
	CNamePlatePart() = delete;
	virtual ~CNamePlatePart() = default;
};

using PartsVector = std::vector<std::unique_ptr<CNamePlatePart>>;

static constexpr ColorRGBA s_OutlineColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);

class CNamePlatePartText : public CNamePlatePart
{
protected:
	STextContainerIndex m_TextContainerIndex;
	virtual bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) = 0;
	virtual void UpdateText(CGameClient &This, const CNamePlateData &Data) = 0;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CNamePlatePartText(CGameClient &This) :
		CNamePlatePart(This)
	{
		Reset(This);
	}

public:
	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		if(!UpdateNeeded(This, Data) && m_TextContainerIndex.Valid())
			return;

		// Set flags
		unsigned int Flags = ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE;
		if(Data.m_InGame)
			Flags |= ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT; // Prevent jittering from rounding
		This.TextRender()->SetRenderFlags(Flags);

		if(Data.m_InGame)
		{
			// Create text at standard zoom
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			This.Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			This.RenderTools()->MapScreenToInterface(This.m_Camera.m_Center.x, This.m_Camera.m_Center.y);
			This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
			UpdateText(This, Data);
			This.Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
		}
		else
		{
			UpdateText(This, Data);
		}

		This.TextRender()->SetRenderFlags(0);

		if(!m_TextContainerIndex.Valid())
		{
			m_Visible = false;
			return;
		}

		const STextBoundingBox Container = This.TextRender()->GetBoundingBoxTextContainer(m_TextContainerIndex);
		m_Size = vec2(Container.m_W, Container.m_H);
	}
	void Reset(CGameClient &This) override
	{
		This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	}
	void Render(CGameClient &This, vec2 Pos) const override
	{
		if(!m_TextContainerIndex.Valid())
			return;

		ColorRGBA OutlineColor, Color;
		Color = m_Color;
		OutlineColor = s_OutlineColor.WithMultipliedAlpha(m_Color.a);
		This.TextRender()->RenderTextContainer(m_TextContainerIndex,
			Color, OutlineColor,
			Pos.x - Size().x / 2.0f, Pos.y - Size().y / 2.0f);
	}
};

class CNamePlatePartIcon : public CNamePlatePart
{
protected:
	IGraphics::CTextureHandle m_Texture;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CNamePlatePartIcon(CGameClient &This) :
		CNamePlatePart(This) {}

public:
	void Render(CGameClient &This, vec2 Pos) const override
	{
		IGraphics::CQuadItem QuadItem(Pos.x - Size().x / 2.0f, Pos.y - Size().y / 2.0f, Size().x, Size().y);
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsBegin();
		This.Graphics()->SetColor(m_Color);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->QuadsDrawTL(&QuadItem, 1);
		This.Graphics()->QuadsEnd();
		This.Graphics()->QuadsSetRotation(0.0f);
	}
};

class CNamePlatePartSprite : public CNamePlatePart
{
protected:
	IGraphics::CTextureHandle m_Texture;
	int m_Sprite = -1;
	int m_SpriteFlags = 0;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CNamePlatePartSprite(CGameClient &This) :
		CNamePlatePart(This) {}

public:
	void Render(CGameClient &This, vec2 Pos) const override
	{
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->QuadsBegin();
		This.Graphics()->SetColor(m_Color);
		This.RenderTools()->SelectSprite(m_Sprite, m_SpriteFlags);
		This.RenderTools()->DrawSprite(Pos.x, Pos.y, Size().x, Size().y);
		This.Graphics()->QuadsEnd();
		This.Graphics()->QuadsSetRotation(0.0f);
	}
};

// Part Definitions

class CNamePlatePartNewLine : public CNamePlatePart
{
public:
	CNamePlatePartNewLine(CGameClient &This) :
		CNamePlatePart(This)
	{
		m_NewLine = true;
	}
};

enum Direction
{
	DIRECTION_LEFT,
	DIRECTION_UP,
	DIRECTION_RIGHT
};

class CNamePlatePartDirection : public CNamePlatePartIcon
{
private:
	int m_Direction;

public:
	CNamePlatePartDirection(CGameClient &This, Direction Dir) :
		CNamePlatePartIcon(This)
	{
		m_Texture = g_pData->m_aImages[IMAGE_ARROW].m_Id;
		m_Direction = Dir;
		switch(m_Direction)
		{
		case DIRECTION_LEFT:
			m_Rotation = pi;
			break;
		case DIRECTION_UP:
			m_Rotation = pi / -2.0f;
			break;
		case DIRECTION_RIGHT:
			m_Rotation = 0.0f;
			break;
		}
	}
	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		if(!Data.m_ShowDirection)
		{
			m_ShiftOnInvis = false;
			m_Visible = false;
			return;
		}
		m_ShiftOnInvis = true; // Only shift (horizontally) the other parts if directions as a whole is visible
		m_Size = vec2(Data.m_FontSizeDirection, Data.m_FontSizeDirection);
		m_Padding.y = m_Size.y / 2.0f;
		switch(m_Direction)
		{
		case DIRECTION_LEFT:
			m_Visible = Data.m_DirLeft;
			break;
		case DIRECTION_UP:
			m_Visible = Data.m_DirJump;
			break;
		case DIRECTION_RIGHT:
			m_Visible = Data.m_DirRight;
			break;
		}
		m_Color.a = Data.m_Color.a;
	}
};

class CNamePlatePartClientId : public CNamePlatePartText
{
private:
	int m_ClientId = -1;
	static_assert(MAX_CLIENTS <= 999, "Make this buffer bigger");
	char m_aText[5] = "";
	float m_FontSize = -INFINITY;
	bool m_ClientIdSeperateLine = false;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowClientId && (Data.m_ClientIdSeperateLine == m_ClientIdSeperateLine);
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClientId || m_ClientId != Data.m_ClientId;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeClientId;
		m_ClientId = Data.m_ClientId;
		if(m_ClientIdSeperateLine)
			str_format(m_aText, sizeof(m_aText), "%d", m_ClientId);
		else
			str_format(m_aText, sizeof(m_aText), "%d:", m_ClientId);
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	CNamePlatePartClientId(CGameClient &This, bool ClientIdSeperateLine) :
		CNamePlatePartText(This)
	{
		m_ClientIdSeperateLine = ClientIdSeperateLine;
	}
};

class CNamePlatePartFriendMark : public CNamePlatePartText
{
private:
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowFriendMark;
		if(!m_Visible)
			return false;
		m_Color.a = Data.m_Color.a;
		return m_FontSize != Data.m_FontSize;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSize;
		CTextCursor Cursor;
		This.TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, FontIcons::FONT_ICON_HEART);
		This.TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}

public:
	CNamePlatePartFriendMark(CGameClient &This) :
		CNamePlatePartText(This)
	{
		m_Color = ColorRGBA(1.0f, 0.0f, 0.0f);
	}
};

class CNamePlatePartName : public CNamePlatePartText
{
private:
	char m_aText[std::max<size_t>(MAX_NAME_LENGTH, protocol7::MAX_NAME_ARRAY_SIZE)] = "";
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowName;
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSize || str_comp(m_aText, Data.m_pName) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSize;
		str_copy(m_aText, Data.m_pName, sizeof(m_aText));
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	CNamePlatePartName(CGameClient &This) :
		CNamePlatePartText(This) {}
};

class CNamePlatePartClan : public CNamePlatePartText
{
private:
	char m_aText[std::max<size_t>(MAX_CLAN_LENGTH, protocol7::MAX_CLAN_ARRAY_SIZE)] = "";
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowClan;
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClan || str_comp(m_aText, Data.m_pClan) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeClan;
		str_copy(m_aText, Data.m_pClan, sizeof(m_aText));
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	CNamePlatePartClan(CGameClient &This) :
		CNamePlatePartText(This) {}
};

class CNamePlatePartHookStrongWeak : public CNamePlatePartSprite
{
protected:
	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeak;
		if(!m_Visible)
			return;
		m_Size = vec2(Data.m_FontSizeHookStrongWeak + DEFAULT_PADDING, Data.m_FontSizeHookStrongWeak + DEFAULT_PADDING);
		switch(Data.m_HookStrongWeakState)
		{
		case EHookStrongWeakState::STRONG:
			m_Sprite = SPRITE_HOOK_STRONG;
			m_Color = color_cast<ColorRGBA>(ColorHSLA(6401973));
			break;
		case EHookStrongWeakState::NEUTRAL:
			m_Sprite = SPRITE_HOOK_ICON;
			m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
			break;
		case EHookStrongWeakState::WEAK:
			m_Sprite = SPRITE_HOOK_WEAK;
			m_Color = color_cast<ColorRGBA>(ColorHSLA(41131));
			break;
		}
		m_Color.a = Data.m_Color.a;
	}

public:
	CNamePlatePartHookStrongWeak(CGameClient &This) :
		CNamePlatePartSprite(This)
	{
		m_Texture = g_pData->m_aImages[IMAGE_STRONGWEAK].m_Id;
		m_Padding = vec2(0.0f, 0.0f);
	}
};

class CNamePlatePartHookStrongWeakId : public CNamePlatePartText
{
private:
	int m_StrongWeakId = -1;
	static_assert(MAX_CLIENTS <= 999, "Make this buffer bigger");
	char m_aText[4] = "";
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeakId;
		if(!m_Visible)
			return false;
		switch(Data.m_HookStrongWeakState)
		{
		case EHookStrongWeakState::STRONG:
			m_Color = color_cast<ColorRGBA>(ColorHSLA(6401973));
			break;
		case EHookStrongWeakState::NEUTRAL:
			m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
			break;
		case EHookStrongWeakState::WEAK:
			m_Color = color_cast<ColorRGBA>(ColorHSLA(41131));
			break;
		}
		m_Color.a = Data.m_Color.a;
		return m_FontSize != Data.m_FontSizeHookStrongWeak || m_StrongWeakId != Data.m_HookStrongWeakId;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeHookStrongWeak;
		m_StrongWeakId = Data.m_HookStrongWeakId;
		str_format(m_aText, sizeof(m_aText), "%d", m_StrongWeakId);
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	CNamePlatePartHookStrongWeakId(CGameClient &This) :
		CNamePlatePartText(This) {}
};

// Name Plates

class CNamePlate
{
private:
	bool m_Inited = false;
	bool m_InGame = false;
	PartsVector m_vpParts;
	void RenderLine(CGameClient &This,
		vec2 Pos, vec2 Size,
		PartsVector::iterator Start, PartsVector::iterator End)
	{
		Pos.x -= Size.x / 2.0f;
		for(auto PartIt = Start; PartIt != End; ++PartIt)
		{
			const CNamePlatePart &Part = **PartIt;
			if(Part.Visible())
			{
				Part.Render(This, vec2(
							  Pos.x + (Part.Padding().x + Part.Size().x) / 2.0f,
							  Pos.y - std::max(Size.y, Part.Padding().y + Part.Size().y) / 2.0f));
			}
			if(Part.Visible() || Part.ShiftOnInvis())
				Pos.x += Part.Size().x + Part.Padding().x;
		}
	}
	template<typename PartType, typename... ArgsType>
	void AddPart(CGameClient &This, ArgsType &&... Args)
	{
		m_vpParts.push_back(std::make_unique<PartType>(This, std::forward<ArgsType>(Args)...));
	}
	void Init(CGameClient &This)
	{
		if(m_Inited)
			return;
		m_Inited = true;

		AddPart<CNamePlatePartDirection>(This, DIRECTION_LEFT);
		AddPart<CNamePlatePartDirection>(This, DIRECTION_UP);
		AddPart<CNamePlatePartDirection>(This, DIRECTION_RIGHT);
		AddPart<CNamePlatePartNewLine>(This);

		AddPart<CNamePlatePartFriendMark>(This);
		AddPart<CNamePlatePartClientId>(This, false);
		AddPart<CNamePlatePartName>(This);
		AddPart<CNamePlatePartNewLine>(This);

		AddPart<CNamePlatePartClan>(This);
		AddPart<CNamePlatePartNewLine>(This);

		AddPart<CNamePlatePartClientId>(This, true);
		AddPart<CNamePlatePartNewLine>(This);

		AddPart<CNamePlatePartHookStrongWeak>(This);
		AddPart<CNamePlatePartHookStrongWeakId>(This);
	}

public:
	CNamePlate() = default;
	CNamePlate(CGameClient &This, const CNamePlateData &Data)
	{
		// Convenience constructor
		Update(This, Data);
	}
	void Reset(CGameClient &This)
	{
		for(auto &Part : m_vpParts)
			Part->Reset(This);
	}
	void Update(CGameClient &This, const CNamePlateData &Data)
	{
		Init(This);
		m_InGame = Data.m_InGame;
		for(auto &Part : m_vpParts)
			Part->Update(This, Data);
	}
	void Render(CGameClient &This, const vec2 &PositionBottomMiddle)
	{
		dbg_assert(m_Inited, "Tried to render uninited nameplate");
		vec2 Position = PositionBottomMiddle;
		// X: Total width including padding of line, Y: Max height of line parts
		vec2 LineSize = vec2(0.0f, 0.0f);
		bool Empty = true;
		auto Start = m_vpParts.begin();
		for(auto PartIt = m_vpParts.begin(); PartIt != m_vpParts.end(); ++PartIt)
		{
			CNamePlatePart &Part = **PartIt;
			if(Part.NewLine())
			{
				if(!Empty)
				{
					RenderLine(This, Position, LineSize, Start, std::next(PartIt));
					Position.y -= LineSize.y;
				}
				Start = std::next(PartIt);
				LineSize = vec2(0.0f, 0.0f);
			}
			else if(Part.Visible() || Part.ShiftOnInvis())
			{
				Empty = false;
				LineSize.x += Part.Size().x + Part.Padding().x;
				LineSize.y = std::max(LineSize.y, Part.Size().y + Part.Padding().y);
			}
		}
		RenderLine(This, Position, LineSize, Start, m_vpParts.end());
		This.Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	vec2 Size() const
	{
		dbg_assert(m_Inited, "Tried to get size of uninited nameplate");
		// X: Total width including padding of line, Y: Max height of line parts
		vec2 LineSize = vec2(0.0f, 0.0f);
		float WMax = 0.0f;
		float HTotal = 0.0f;
		bool Empty = true;
		for(auto PartIt = m_vpParts.begin(); PartIt != m_vpParts.end(); ++PartIt) // NOLINT(modernize-loop-convert) For consistency with Render
		{
			CNamePlatePart &Part = **PartIt;
			if(Part.NewLine())
			{
				if(!Empty)
				{
					if(LineSize.x > WMax)
						WMax = LineSize.x;
					HTotal += LineSize.y;
				}
				LineSize = vec2(0.0f, 0.0f);
			}
			else if(Part.Visible() || Part.ShiftOnInvis())
			{
				Empty = false;
				LineSize.x += Part.Size().x + Part.Padding().x;
				LineSize.y = std::max(LineSize.y, Part.Size().y + Part.Padding().y);
			}
		}
		if(LineSize.x > WMax)
			WMax = LineSize.x;
		HTotal += LineSize.y;
		return vec2(WMax, HTotal);
	}
};

class CNamePlates::CNamePlatesData
{
public:
	CNamePlate m_aNamePlates[MAX_CLIENTS];
};

void CNamePlates::RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha)
{
	// Get screen edges to avoid rendering offscreen
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// Assume that the name plate fits into a 800x800 box placed directly above the tee
	ScreenX0 -= 400;
	ScreenX1 += 400;
	ScreenY1 += 800;
	if(!(in_range(Position.x, ScreenX0, ScreenX1) && in_range(Position.y, ScreenY0, ScreenY1)))
		return;

	CNamePlateData Data;

	const auto &ClientData = GameClient()->m_aClients[pPlayerInfo->m_ClientId];
	const bool OtherTeam = GameClient()->IsOtherTeam(pPlayerInfo->m_ClientId);

	Data.m_InGame = true;

	Data.m_ShowName = pPlayerInfo->m_Local ? g_Config.m_ClNamePlatesOwn : g_Config.m_ClNamePlates;
	Data.m_pName = GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aName;
	Data.m_ShowFriendMark = Data.m_ShowName && g_Config.m_ClNamePlatesFriendMark && GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_Friend;
	Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_FontSize = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;

	Data.m_ClientId = pPlayerInfo->m_ClientId;
	Data.m_ClientIdSeperateLine = g_Config.m_ClNamePlatesIdsSeperateLine;
	Data.m_FontSizeClientId = Data.m_ClientIdSeperateLine ? (18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f) : Data.m_FontSize;

	Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan;
	Data.m_pClan = GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aClan;
	Data.m_FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;

	Data.m_FontSizeHookStrongWeak = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;
	Data.m_FontSizeDirection = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;

	if(g_Config.m_ClNamePlatesAlways == 0)
		Alpha *= std::clamp(1.0f - std::pow(distance(GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy], Position) / 200.0f, 16.0f), 0.0f, 1.0f);
	if(OtherTeam)
		Alpha *= (float)g_Config.m_ClShowOthersAlpha / 100.0f;

	Data.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
	if(g_Config.m_ClNamePlatesTeamcolors)
	{
		if(GameClient()->IsTeamPlay())
		{
			if(ClientData.m_Team == TEAM_RED)
				Data.m_Color = ColorRGBA(1.0f, 0.5f, 0.5f);
			else if(ClientData.m_Team == TEAM_BLUE)
				Data.m_Color = ColorRGBA(0.7f, 0.7f, 1.0f);
		}
		else
		{
			const int Team = GameClient()->m_Teams.Team(pPlayerInfo->m_ClientId);
			if(Team)
				Data.m_Color = GameClient()->GetDDTeamColor(Team, 0.75f);
		}
	}
	Data.m_Color.a = Alpha;

	int ShowDirectionConfig = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ShowDirectionConfig = g_Config.m_ClVideoShowDirection;
#endif
	Data.m_DirLeft = Data.m_DirJump = Data.m_DirRight = false;
	switch(ShowDirectionConfig)
	{
	case 0: // Off
		Data.m_ShowDirection = false;
		break;
	case 1: // Others
		Data.m_ShowDirection = !pPlayerInfo->m_Local;
		break;
	case 2: // Everyone
		Data.m_ShowDirection = true;
		break;
	case 3: // Only self
		Data.m_ShowDirection = pPlayerInfo->m_Local;
		break;
	default:
		dbg_assert(false, "ShowDirectionConfig invalid");
		dbg_break();
	}
	if(Data.m_ShowDirection)
	{
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pPlayerInfo->m_ClientId == GameClient()->m_aLocalIds[!g_Config.m_ClDummy])
		{
			const auto &InputData = GameClient()->m_Controls.m_aInputData[!g_Config.m_ClDummy];
			Data.m_DirLeft = InputData.m_Direction == -1;
			Data.m_DirJump = InputData.m_Jump == 1;
			Data.m_DirRight = InputData.m_Direction == 1;
		}
		else if(Client()->State() != IClient::STATE_DEMOPLAYBACK && pPlayerInfo->m_Local) // Always render local input when not in demo playback
		{
			const auto &InputData = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
			Data.m_DirLeft = InputData.m_Direction == -1;
			Data.m_DirJump = InputData.m_Jump == 1;
			Data.m_DirRight = InputData.m_Direction == 1;
		}
		else
		{
			const auto &Character = GameClient()->m_Snap.m_aCharacters[pPlayerInfo->m_ClientId];
			Data.m_DirLeft = Character.m_Cur.m_Direction == -1;
			Data.m_DirJump = Character.m_Cur.m_Jumped & 1;
			Data.m_DirRight = Character.m_Cur.m_Direction == 1;
		}
	}

	Data.m_ShowHookStrongWeak = false;
	Data.m_HookStrongWeakState = EHookStrongWeakState::NEUTRAL;
	Data.m_ShowHookStrongWeakId = false;
	Data.m_HookStrongWeakId = 0;

	const bool Following = (GameClient()->m_Snap.m_SpecInfo.m_Active && !GameClient()->m_MultiViewActivated && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW);
	if(GameClient()->m_Snap.m_LocalClientId != -1 || Following)
	{
		const int SelectedId = Following ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
		const CGameClient::CSnapState::CCharacterInfo &Selected = GameClient()->m_Snap.m_aCharacters[SelectedId];
		const CGameClient::CSnapState::CCharacterInfo &Other = GameClient()->m_Snap.m_aCharacters[pPlayerInfo->m_ClientId];

		if((Selected.m_HasExtendedData || GameClient()->m_aClients[SelectedId].m_SpecCharPresent) && Other.m_HasExtendedData)
		{
			int SelectedStrongWeakId = Selected.m_HasExtendedData ? Selected.m_ExtendedData.m_StrongWeakId : 0;
			Data.m_HookStrongWeakId = Other.m_ExtendedData.m_StrongWeakId;
			Data.m_ShowHookStrongWeakId = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong == 2;
			if(SelectedId == pPlayerInfo->m_ClientId)
				Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
			else
			{
				Data.m_HookStrongWeakState = SelectedStrongWeakId > Other.m_ExtendedData.m_StrongWeakId ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;
				Data.m_ShowHookStrongWeak = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong > 0;
			}
		}
	}

	// Check if the nameplate is actually on screen
	CNamePlate &NamePlate = m_pData->m_aNamePlates[pPlayerInfo->m_ClientId];
	NamePlate.Update(*GameClient(), Data);
	NamePlate.Render(*GameClient(), Position - vec2(0.0f, (float)g_Config.m_ClNamePlatesOffset));
}

void CNamePlates::RenderNamePlatePreview(vec2 Position, int Dummy)
{
	const float FontSize = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;
	const float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;

	const float FontSizeDirection = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;
	const float FontSizeHookStrongWeak = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;

	CNamePlateData Data;

	Data.m_InGame = false;
	Data.m_Color = g_Config.m_ClNamePlatesTeamcolors ? GameClient()->GetDDTeamColor(13, 0.75f) : TextRender()->DefaultTextColor();
	Data.m_Color.a = 1.0f;

	Data.m_ShowName = g_Config.m_ClNamePlates || g_Config.m_ClNamePlatesOwn;
	Data.m_pName = Dummy == 0 ? Client()->PlayerName() : Client()->DummyName();
	Data.m_FontSize = FontSize;

	Data.m_ShowFriendMark = Data.m_ShowName && g_Config.m_ClNamePlatesFriendMark;

	Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_ClientId = Dummy + 1;
	Data.m_ClientIdSeperateLine = g_Config.m_ClNamePlatesIdsSeperateLine;
	Data.m_FontSizeClientId = Data.m_ClientIdSeperateLine ? (18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f) : Data.m_FontSize;

	Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan;
	Data.m_pClan = Dummy == 0 ? g_Config.m_PlayerClan : g_Config.m_ClDummyClan;
	if(!Data.m_pClan[0])
		Data.m_pClan = "Clan Name";
	Data.m_FontSizeClan = FontSizeClan;

	Data.m_ShowDirection = g_Config.m_ClShowDirection != 0 ? true : false;
	Data.m_DirLeft = Data.m_DirJump = Data.m_DirRight = true;
	Data.m_FontSizeDirection = FontSizeDirection;

	Data.m_FontSizeHookStrongWeak = FontSizeHookStrongWeak;
	Data.m_HookStrongWeakId = Data.m_ClientId;
	Data.m_ShowHookStrongWeakId = g_Config.m_ClNamePlatesStrong == 2;
	if(Dummy == g_Config.m_ClDummy)
	{
		Data.m_HookStrongWeakState = EHookStrongWeakState::NEUTRAL;
		Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
	}
	else
	{
		Data.m_HookStrongWeakState = Data.m_HookStrongWeakId == 2 ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;
		Data.m_ShowHookStrongWeak = g_Config.m_ClNamePlatesStrong > 0;
	}

	CTeeRenderInfo TeeRenderInfo;
	if(Dummy == 0)
	{
		TeeRenderInfo.Apply(m_pClient->m_Skins.Find(g_Config.m_ClPlayerSkin));
		TeeRenderInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
	}
	else
	{
		TeeRenderInfo.Apply(m_pClient->m_Skins.Find(g_Config.m_ClDummySkin));
		TeeRenderInfo.ApplyColors(g_Config.m_ClDummyUseCustomColor, g_Config.m_ClDummyColorBody, g_Config.m_ClDummyColorFeet);
	}
	TeeRenderInfo.m_Size = 64.0f;

	CNamePlate NamePlate(*GameClient(), Data);
	Position.y += NamePlate.Size().y / 2.0f;
	Position.y += (float)g_Config.m_ClNamePlatesOffset / 2.0f;
	vec2 Dir = Ui()->MousePos() - Position;
	Dir /= TeeRenderInfo.m_Size;
	const float Length = length(Dir);
	if(Length > 1.0f)
		Dir /= Length;
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, 0, Dir, Position);
	Position.y -= (float)g_Config.m_ClNamePlatesOffset;
	NamePlate.Render(*GameClient(), Position - vec2(0.0f, (float)g_Config.m_ClNamePlatesOffset));
	NamePlate.Reset(*GameClient());
}

void CNamePlates::ResetNamePlates()
{
	for(CNamePlate &NamePlate : m_pData->m_aNamePlates)
		NamePlate.Reset(*GameClient());
}

void CNamePlates::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	int ShowDirection = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ShowDirection = g_Config.m_ClVideoShowDirection;
#endif
	if(!g_Config.m_ClNamePlates && ShowDirection == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apPlayerInfos[i];
		if(!pInfo)
			continue;

		// Each player can also have a spectator char whose name plate is displayed independently
		if(GameClient()->m_aClients[i].m_SpecCharPresent)
		{
			const vec2 RenderPos = GameClient()->m_aClients[i].m_SpecChar;
			RenderNamePlateGame(RenderPos, pInfo, 0.4f);
		}
		// Only render name plates for active characters
		if(GameClient()->m_Snap.m_aCharacters[i].m_Active)
		{
			const vec2 RenderPos = GameClient()->m_aClients[i].m_RenderPos;
			RenderNamePlateGame(RenderPos, pInfo, 1.0f);
		}
	}
}

void CNamePlates::OnWindowResize()
{
	ResetNamePlates();
}

CNamePlates::CNamePlates() :
	m_pData(new CNamePlates::CNamePlatesData()) {}

CNamePlates::~CNamePlates()
{
	delete m_pData;
}
