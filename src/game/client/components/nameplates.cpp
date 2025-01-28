/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include "camera.h"
#include "controls.h"
#include "nameplates.h"

// Part Types

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

using PartsVector = std::vector<std::unique_ptr<CNamePlatePart>>;

class CNamePlatePartText : public CNamePlatePart
{
protected:
	STextContainerIndex m_TextContainerIndex;
	virtual bool UpdateNeeded(CGameClient &This, const CNamePlateRenderData &Data) { return true; };
	virtual void UpdateText(CGameClient &This, const CNamePlateRenderData &Data) = 0;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	void Create(CGameClient &This)
	{
		Reset(This);
	}

public:
	friend class CGameClient;
	void Update(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		if(!UpdateNeeded(This, Data) && m_TextContainerIndex.Valid())
			return;

		if(Data.m_InGame)
		{
			// create namePlates at standard zoom
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			This.Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			This.RenderTools()->MapScreenToInterface(This.m_Camera.m_Center.x, This.m_Camera.m_Center.y);
			This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
			UpdateText(This, Data);
			This.Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
		}
		else
			UpdateText(This, Data);

		if(m_TextContainerIndex.Valid())
		{
			auto Container = This.TextRender()->GetBoundingBoxTextContainer(m_TextContainerIndex);
			m_Width = Container.m_W;
			m_Height = Container.m_H;
		}
		else
			m_Visible = false;
	}
	void Reset(CGameClient &This) override
	{
		This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	}
	void Render(CGameClient &This, float X, float Y) override
	{
		if(!m_TextContainerIndex.Valid())
			return;

		ColorRGBA OutlineColor = This.TextRender()->DefaultTextOutlineColor();
		OutlineColor.a *= m_Color.a;

		This.TextRender()->RenderTextContainer(m_TextContainerIndex,
			m_Color, OutlineColor,
			X - Width() / 2.0f, Y - Height() / 2.0f);
	}
};

class CNamePlatePartIcon : public CNamePlatePart
{
protected:
	int m_QuadContainerIndex;
	IGraphics::CTextureHandle m_Texture;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	void Create(CGameClient &This)
	{
		m_QuadContainerIndex = This.Graphics()->CreateQuadContainer(false);
		This.RenderTools()->QuadContainerAddSprite(m_QuadContainerIndex, 0.0f, 0.0f, 1.0f);
		This.Graphics()->QuadContainerUpload(m_QuadContainerIndex);
	}

public:
	friend class CGameClient;
	void Render(CGameClient &This, float X, float Y) override
	{
		This.Graphics()->SetColor(m_Color);
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->RenderQuadContainerAsSprite(m_QuadContainerIndex, 0, X - Width() / 2.0f, Y - Height() / 2.0f, Width(), Height());
		This.Graphics()->QuadsSetRotation(0.0f);
		This.Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
};

class CNamePlatePartSprite : public CNamePlatePart
{
protected:
	IGraphics::CTextureHandle m_Texture;
	int m_Sprite = 0;
	int m_SpriteFlags = 0;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	void Create(CGameClient &This) {}

public:
	friend class CGameClient;
	void Render(CGameClient &This, float X, float Y) override
	{
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->QuadsBegin();
		This.Graphics()->SetColor(m_Color);
		This.RenderTools()->SelectSprite(m_Sprite, m_SpriteFlags);
		This.RenderTools()->DrawSprite(X, Y, Width(), Height());
		This.Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		This.Graphics()->QuadsEnd();
		This.Graphics()->QuadsSetRotation(0.0f);
	}
};

// Part Definitions

class CNamePlatePartNewLine : public CNamePlatePart
{
public:
	void Create(CGameClient &This) {}
	CNamePlatePartNewLine()
	{
		m_NewLine = true;
	}
};

class CNamePlatePartDirection : public CNamePlatePartIcon
{
private:
	int m_Direction;

public:
	void Create(CGameClient &This, int Direction)
	{
		CNamePlatePartIcon::Create(This);
		m_Texture = g_pData->m_aImages[IMAGE_ARROW].m_Id;
		m_ShiftOnInvis = true;
		m_Direction = Direction;
		switch(m_Direction)
		{
		case 0:
			m_Rotation = pi;
			break;
		case 1:
			m_Rotation = pi / -2.0f;
			break;
		case 2:
			m_Rotation = 0.0f;
			break;
		}
	}
	void Update(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		if(!Data.m_ShowDirection)
		{
			m_Width = m_Height = 0;
			m_Visible = false;
			return;
		}
		m_Width = m_Height = Data.m_FontSizeDirection;
		switch(m_Direction)
		{
		case 0:
			m_Visible = Data.m_DirLeft;
			m_OffsetY = m_Height / 2.0f;
			break;
		case 1:
			m_Visible = Data.m_DirJump;
			m_OffsetY = m_Height / -2.0f;
			break;
		case 2:
			m_Visible = Data.m_DirRight;
			m_OffsetY = m_Height / 2.0f;
			break;
		}
	}
};

class CNamePlatePartClientId : public CNamePlatePartText
{
private:
	int m_ClientId = -1;
	static_assert(MAX_CLIENTS <= 999, "Make this buffer bigger");
	char m_aText[5] = "";
	float m_FontSize = -INFINITY;
	bool m_ClientIdNewLine = false;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowClientId && (Data.m_ClientIdNewLine == m_ClientIdNewLine);
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClientId || m_ClientId != Data.m_ClientId;
	}
	void UpdateText(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_FontSize = Data.m_FontSizeClientId;
		m_ClientId = Data.m_ClientId;
		str_format(m_aText, sizeof(m_aText), m_ClientIdNewLine ? "%d" : "%d:", m_ClientId);
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	void Create(CGameClient &This, bool ClientIdNewLine)
	{
		CNamePlatePartText::Create(This);
		m_ClientIdNewLine = ClientIdNewLine;
	}
};

class CNamePlatePartFriendMark : public CNamePlatePartIcon
{
public:
	void Update(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowFriendMark;
		if(!m_Visible)
			return;
		m_Texture = This.m_GameSkin.m_SpriteHealthFull;
		m_Width = m_Height = Data.m_FontSize;
		m_Color.a = Data.m_Alpha;
	}

	void Create(CGameClient &This)
	{
		CNamePlatePartIcon::Create(This);
	}
};

class CNamePlatePartName : public CNamePlatePartText
{
private:
	char m_aText[MAX_NAME_LENGTH] = "";
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowName;
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSize || str_comp(m_aText, Data.m_pName) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_FontSize = Data.m_FontSize;
		str_copy(m_aText, Data.m_pName, sizeof(m_aText));
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	void Create(CGameClient &This)
	{
		CNamePlatePartText::Create(This);
	}
};

class CNamePlatePartClan : public CNamePlatePartText
{
private:
	char m_aText[MAX_CLAN_LENGTH] = "";
	float m_FontSize = -INFINITY;

protected:
	bool UpdateNeeded(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowClan;
		if(!m_Visible)
			return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClan || str_comp(m_aText, Data.m_pClan) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_FontSize = Data.m_FontSizeClan;
		str_copy(m_aText, Data.m_pClan, sizeof(m_aText));
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	void Create(CGameClient &This)
	{
		CNamePlatePartText::Create(This);
	}
};

class CNamePlatePartHookStrongWeak : public CNamePlatePartSprite
{
protected:
	void Update(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeak;
		if(!m_Visible)
			return;
		m_Width = m_Height = Data.m_FontSizeHookStrongWeak * 1.5f;
		switch(Data.m_HookStrongWeak)
		{
		case CNamePlateRenderData::HOOKSTRONGWEAK_STRONG:
			m_Sprite = SPRITE_HOOK_STRONG;
			m_Color = color_cast<ColorRGBA>(ColorHSLA(6401973));
			break;
		case CNamePlateRenderData::HOOKSTRONGWEAK_UNKNOWN:
			m_Sprite = SPRITE_HOOK_ICON;
			m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
			break;
		case CNamePlateRenderData::HOOKSTRONGWEAK_WEAK:
			m_Sprite = SPRITE_HOOK_WEAK;
			m_Color = color_cast<ColorRGBA>(ColorHSLA(41131));
			break;
		}
		m_Color.a = Data.m_Alpha;
	}

public:
	void Create(CGameClient &This)
	{
		CNamePlatePartSprite::Create(This);
		m_Texture = g_pData->m_aImages[IMAGE_STRONGWEAK].m_Id;
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
	bool UpdateNeeded(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeakId;
		if(!m_Visible)
			return false;
		m_Color.a = Data.m_Alpha;
		return m_FontSize != Data.m_FontSizeHookStrongWeak || m_StrongWeakId != Data.m_HookStrongWeakId;
	}
	void UpdateText(CGameClient &This, const CNamePlateRenderData &Data) override
	{
		m_FontSize = Data.m_FontSizeHookStrongWeak;
		m_StrongWeakId = Data.m_ClientId;
		switch(Data.m_HookStrongWeak)
		{
		case CNamePlateRenderData::HOOKSTRONGWEAK_STRONG:
			m_Color = color_cast<ColorRGBA>(ColorHSLA(6401973));
			break;
		case CNamePlateRenderData::HOOKSTRONGWEAK_UNKNOWN:
			m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
			break;
		case CNamePlateRenderData::HOOKSTRONGWEAK_WEAK:
			m_Color = color_cast<ColorRGBA>(ColorHSLA(41131));
			break;
		}
		m_Color.a = Data.m_Alpha;
		str_format(m_aText, sizeof(m_aText), "%d", m_StrongWeakId);
		CTextCursor Cursor;
		This.TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize, TEXTFLAG_RENDER);
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	void Create(CGameClient &This)
	{
		CNamePlatePartText::Create(This);
	}
};

class CNamePlate
{
private:
	bool m_Inited = false;
	PartsVector m_vpParts;
	void RenderLine(CGameClient &This, const CNamePlateRenderData &Data,
		float X, float Y, float W, float H,
		PartsVector::iterator Start, PartsVector::iterator End)
	{
		X -= W / 2.0f;
		for(auto Part = Start; Part != End; ++Part)
		{
			if((*Part)->Visible())
			{
				float PartX = X + ((*Part)->PaddingX() + (*Part)->Width()) / 2.0f + (*Part)->OffsetX();
				float PartY = Y - std::max(H, (*Part)->PaddingY() + (*Part)->Height()) / 2.0f + (*Part)->OffsetY();
				(*Part)->Render(This, PartX, PartY);
				// Debug
				// This.Graphics()->TextureClear();
				// This.Graphics()->QuadsBegin();
				// This.Graphics()->SetColor(1.0f, (*Part)->ShiftOnInvis() ? 1.0f : 0.0f, 0.0f, 0.5f);
				// This.Graphics()->DrawCircle(PartX, PartY, 3, 10);
				// This.Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				// This.Graphics()->QuadsEnd();
			}
			if((*Part)->Visible() || (*Part)->ShiftOnInvis())
				X += (*Part)->Width() + (*Part)->PaddingX();
		}
	}
	template<typename PartType, typename... ArgsType>
	void AddPart(CGameClient &This, ArgsType &&... Args)
	{
		std::unique_ptr<PartType> Part = std::make_unique<PartType>();
		Part->Create(This, std::forward<ArgsType>(Args)...);
		m_vpParts.push_back(std::unique_ptr<CNamePlatePart>(std::move(Part)));
	}
	void Init(CGameClient &This)
	{
		if(m_Inited)
			return;
		m_Inited = true;

		AddPart<CNamePlatePartDirection>(This, 0);
		AddPart<CNamePlatePartDirection>(This, 1);
		AddPart<CNamePlatePartDirection>(This, 2);
		AddPart<CNamePlatePartNewLine>(This);
		AddPart<CNamePlatePartClientId>(This, false);
		AddPart<CNamePlatePartFriendMark>(This);
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
	friend class CGameClient;
	void Reset(CGameClient &This)
	{
		for(auto &Part : m_vpParts)
			Part->Reset(This);
	}
	void Render(CGameClient &This, const CNamePlateRenderData &Data)
	{
		Init(This);
		int Flags = ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE;
		if(Data.m_InGame)
			Flags |= ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT;
		This.TextRender()->SetRenderFlags(Flags);
		float X = Data.m_Position.x;
		float Y = Data.m_Position.y - 50.0f;
		float W = 0.0f; // Total width including padding of line
		float H = 0.0f; // Max height of line parts
		bool Empty = true;
		auto Start = m_vpParts.begin();
		for(auto Part = m_vpParts.begin(); Part != m_vpParts.end(); ++Part)
		{
			(*Part)->Update(This, Data);
			if((*Part)->NewLine())
			{
				if(!Empty)
				{
					RenderLine(This, Data, X, Y, W, H, Start, std::next(Part));
					Y -= H;
				}
				Start = std::next(Part);
				W = 0.0f;
				H = 0.0f;
			}
			else if((*Part)->Visible() || (*Part)->ShiftOnInvis())
			{
				Empty = false;
				W += (*Part)->Width() + (*Part)->PaddingX();
				H = std::max(H, (*Part)->Height() + (*Part)->PaddingY());
			}
		}
		RenderLine(This, Data, X, Y, W, H, Start, m_vpParts.end());
		This.TextRender()->SetRenderFlags(0);
	}
};

void CNamePlates::RenderNamePlate(CNamePlate &NamePlate, const CNamePlateRenderData &Data)
{
	NamePlate.Render(*GameClient(), Data);
}

void CNamePlates::RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool ForceAlpha)
{
	CNamePlateRenderData Data;

	const auto &ClientData = GameClient()->m_aClients[pPlayerInfo->m_ClientId];
	const bool OtherTeam = GameClient()->IsOtherTeam(pPlayerInfo->m_ClientId);

	bool ShowNamePlate = g_Config.m_ClNamePlates && (!pPlayerInfo->m_Local || g_Config.m_ClNamePlatesOwn);

	Data.m_InGame = true;
	Data.m_Position = Position;
	Data.m_ShowName = ShowNamePlate;
	Data.m_pName = GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aName;
	Data.m_ShowFriendMark = ShowNamePlate && g_Config.m_ClNamePlatesFriendMark && GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_Friend;
	Data.m_ShowClientId = ShowNamePlate && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_FontSize = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;

	Data.m_ClientId = pPlayerInfo->m_ClientId;
	Data.m_ClientIdNewLine = g_Config.m_ClNamePlatesIdsNewLine;
	Data.m_FontSizeClientId = Data.m_ClientIdNewLine ? (18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f) : Data.m_FontSize;

	Data.m_ShowClan = ShowNamePlate && g_Config.m_ClNamePlatesClan;
	Data.m_pClan = GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aClan;
	Data.m_FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;

	Data.m_FontSizeHookStrongWeak = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;
	Data.m_FontSizeDirection = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;

	Data.m_Alpha = Alpha;
	if(!ForceAlpha)
	{
		if(g_Config.m_ClNamePlatesAlways == 0)
			Data.m_Alpha *= clamp(1.0f - std::pow(distance(GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy], Position) / 200.0f, 16.0f), 0.0f, 1.0f);
		if(OtherTeam)
			Data.m_Alpha *= (float)g_Config.m_ClShowOthersAlpha / 100.0f;
	}

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
	Data.m_Color.a = Data.m_Alpha;

	int ShowDirectionConfig = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ShowDirectionConfig = g_Config.m_ClVideoShowDirection;
#endif
	Data.m_DirLeft = Data.m_DirJump = Data.m_DirRight = false;
	switch(ShowDirectionConfig)
	{
	case 0: // off
		Data.m_ShowDirection = false;
		break;
	case 1: // others
		Data.m_ShowDirection = !pPlayerInfo->m_Local;
		break;
	case 2: // everyone
		Data.m_ShowDirection = true;
		break;
	case 3: // only self
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
		else if(Client()->State() != IClient::STATE_DEMOPLAYBACK && pPlayerInfo->m_Local) // always render local input when not in demo playback
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
	Data.m_HookStrongWeak = CNamePlateRenderData::HOOKSTRONGWEAK_UNKNOWN;
	Data.m_ShowHookStrongWeakId = false;
	Data.m_HookStrongWeakId = 0;

	const bool Following = (GameClient()->m_Snap.m_SpecInfo.m_Active && !GameClient()->m_MultiViewActivated && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW);
	if(GameClient()->m_Snap.m_LocalClientId != -1 || Following)
	{
		const int SelectedId = Following ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
		const CGameClient::CSnapState::CCharacterInfo &Selected = GameClient()->m_Snap.m_aCharacters[SelectedId];
		const CGameClient::CSnapState::CCharacterInfo &Other = GameClient()->m_Snap.m_aCharacters[pPlayerInfo->m_ClientId];
		if(Selected.m_HasExtendedData && Other.m_HasExtendedData)
		{
			Data.m_HookStrongWeakId = Other.m_ExtendedData.m_StrongWeakId;
			Data.m_ShowHookStrongWeakId = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong == 2;
			if(SelectedId == pPlayerInfo->m_ClientId)
				Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
			else
			{
				Data.m_HookStrongWeak = Selected.m_ExtendedData.m_StrongWeakId > Other.m_ExtendedData.m_StrongWeakId ? CNamePlateRenderData::HOOKSTRONGWEAK_STRONG : CNamePlateRenderData::HOOKSTRONGWEAK_WEAK;
				Data.m_ShowHookStrongWeak = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong > 0;
			}
		}
	}

	GameClient()->m_NamePlates.RenderNamePlate(m_aNamePlates[pPlayerInfo->m_ClientId], Data);
}

void CNamePlates::RenderNamePlatePreview(vec2 Position, int Dummy)
{
	const float FontSize = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;
	const float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;

	const float FontSizeDirection = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;
	const float FontSizeHookStrongWeak = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;

	CNamePlateRenderData Data;

	Data.m_InGame = false;
	Data.m_Position = Position;
	Data.m_Color = g_Config.m_ClNamePlatesTeamcolors ? GameClient()->GetDDTeamColor(13, 0.75f) : TextRender()->DefaultTextColor();
	Data.m_Alpha = 1.0f;

	Data.m_ShowName = g_Config.m_ClNamePlates;
	Data.m_pName = Dummy == 0 ? Client()->PlayerName() : Client()->DummyName();
	Data.m_FontSize = FontSize;

	Data.m_ShowFriendMark = g_Config.m_ClNamePlates && g_Config.m_ClNamePlatesFriendMark;

	Data.m_ShowClientId = g_Config.m_ClNamePlates && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_ClientId = Dummy + 1;
	Data.m_ClientIdNewLine = g_Config.m_ClNamePlatesIdsNewLine;
	Data.m_FontSizeClientId = Data.m_ClientIdNewLine ? (18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f) : Data.m_FontSize;

	Data.m_ShowClan = g_Config.m_ClNamePlates && g_Config.m_ClNamePlatesClan;
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
		Data.m_HookStrongWeak = CNamePlateRenderData::HOOKSTRONGWEAK_UNKNOWN;
		Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
	}
	else
	{
		Data.m_HookStrongWeak = Data.m_HookStrongWeakId == 2 ? CNamePlateRenderData::HOOKSTRONGWEAK_STRONG : CNamePlateRenderData::HOOKSTRONGWEAK_WEAK;
		Data.m_ShowHookStrongWeak = g_Config.m_ClNamePlatesStrong > 0;
	}

	CNamePlate NamePlate;
	GameClient()->m_NamePlates.RenderNamePlate(NamePlate, Data);
	NamePlate.Reset(*GameClient());
}

void CNamePlates::ResetNamePlates()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		m_aNamePlates[i].Reset(*GameClient());
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

	// get screen edges to avoid rendering offscreen
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	// expand the edges to prevent popping in/out onscreen
	// it is assumed that the NamePlate and all its components fit into a 800x800 box placed directly above the tee
	// this may need to be changed or calculated differently in the future
	ScreenX0 -= 400;
	ScreenX1 += 400;
	// ScreenY0 -= 0;
	ScreenY1 += 800;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apPlayerInfos[i];
		if(!pInfo)
			continue;

		if(GameClient()->m_aClients[i].m_SpecCharPresent)
		{
			// Each player can also have a spec char whose name plate is displayed independently
			const vec2 RenderPos = GameClient()->m_aClients[i].m_SpecChar;
			// don't render offscreen
			if(in_range(RenderPos.x, ScreenX0, ScreenX1) && in_range(RenderPos.y, ScreenY0, ScreenY1))
				RenderNamePlateGame(RenderPos, pInfo, 0.4f, true);
		}
		if(GameClient()->m_Snap.m_aCharacters[i].m_Active)
		{
			// Only render name plates for active characters
			const vec2 RenderPos = GameClient()->m_aClients[i].m_RenderPos;
			// don't render offscreen
			if(in_range(RenderPos.x, ScreenX0, ScreenX1) && in_range(RenderPos.y, ScreenY0, ScreenY1))
				RenderNamePlateGame(RenderPos, pInfo, 1.0f, false);
		}
	}
}

void CNamePlates::OnWindowResize()
{
	ResetNamePlates();
}

void CNamePlates::OnInit()
{
	m_aNamePlates = new CNamePlate[MAX_CLIENTS];
}

CNamePlates::~CNamePlates()
{
	delete[] m_aNamePlates;
}
