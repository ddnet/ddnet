#include "proof_mode.h"

#include <game/client/components/menu_background.h>

#include "editor.h"

void CProofMode::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);
	SetMenuBackgroundPositionNames();
	OnReset();
	OnMapLoad();
}

void CProofMode::OnReset()
{
	m_ProofBorders = PROOF_BORDER_OFF;
	m_CurrentMenuProofIndex = 0;
}

void CProofMode::OnMapLoad()
{
	m_vMenuBackgroundCollisions = {};
	ResetMenuBackgroundPositions();
}

void CProofMode::SetMenuBackgroundPositionNames()
{
	m_vpMenuBackgroundPositionNames.resize(CMenuBackground::NUM_POS);
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_START] = "start";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_INTERNET] = "browser(internet)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_LAN] = "browser(lan)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_DEMOS] = "demos";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_NEWS] = "news";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_FAVORITES] = "favorites";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_LANGUAGE] = "settings(language)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_GENERAL] = "settings(general)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_PLAYER] = "settings(player)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_TEE] = "settings(tee)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_APPEARANCE] = "settings(appearance)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_CONTROLS] = "settings(controls)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_GRAPHICS] = "settings(graphics)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_SOUND] = "settings(sound)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_DDNET] = "settings(ddnet)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_ASSETS] = "settings(assets)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM0] = "custom(1)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM1] = "custom(2)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM2] = "custom(3)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM3] = "custom(4)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_BROWSER_CUSTOM4] = "custom(5)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_RESERVED0] = "reserved settings(1)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_SETTINGS_RESERVED1] = "reserved settings(2)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED0] = "reserved(1)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED1] = "reserved(2)";
	m_vpMenuBackgroundPositionNames[CMenuBackground::POS_RESERVED2] = "reserved(3)";
}

void CProofMode::ResetMenuBackgroundPositions()
{
	std::array<vec2, CMenuBackground::NUM_POS> aBackgroundPositions = GenerateMenuBackgroundPositions();
	m_vMenuBackgroundPositions.assign(aBackgroundPositions.begin(), aBackgroundPositions.end());

	if(Editor()->m_Map.m_pGameLayer)
	{
		for(int y = 0; y < Editor()->m_Map.m_pGameLayer->m_Height; ++y)
		{
			for(int x = 0; x < Editor()->m_Map.m_pGameLayer->m_Width; ++x)
			{
				CTile Tile = Editor()->m_Map.m_pGameLayer->GetTile(x, y);
				if(Tile.m_Index >= TILE_TIME_CHECKPOINT_FIRST && Tile.m_Index <= TILE_TIME_CHECKPOINT_LAST)
				{
					int ArrayIndex = clamp<int>((Tile.m_Index - TILE_TIME_CHECKPOINT_FIRST), 0, CMenuBackground::NUM_POS);
					m_vMenuBackgroundPositions[ArrayIndex] = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				}

				x += Tile.m_Skip;
			}
		}
	}

	m_vMenuBackgroundCollisions.clear();
	m_vMenuBackgroundCollisions.resize(m_vMenuBackgroundPositions.size());
	for(size_t i = 0; i < m_vMenuBackgroundPositions.size(); i++)
	{
		for(size_t j = i + 1; j < m_vMenuBackgroundPositions.size(); j++)
		{
			if(i != j && distance(m_vMenuBackgroundPositions[i], m_vMenuBackgroundPositions[j]) < 0.001f)
				m_vMenuBackgroundCollisions.at(i).push_back(j);
		}
	}
}

void CProofMode::RenderScreenSizes()
{
	const vec2 WorldOffset = Editor()->MapView()->GetWorldOffset();

	// render screen sizes
	if(m_ProofBorders != PROOF_BORDER_OFF && !Editor()->MapView()->m_ShowPicker)
	{
		std::shared_ptr<CLayerGroup> pGameGroup = Editor()->m_Map.m_pGameGroup;
		pGameGroup->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		// possible screen sizes (white border)
		float aLastPoints[4];
		float Start = 1.0f; // 9.0f/16.0f;
		float End = 16.0f / 9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End - Start) * (i / (float)NumSteps);

			float Zoom = (m_ProofBorders == PROOF_BORDER_MENU) ? 0.7f : 1.0f;
			RenderTools()->MapScreenToWorld(
				WorldOffset.x, WorldOffset.y,
				100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, Zoom, aPoints);

			if(i == 0)
			{
				IGraphics::CLineItem aArray[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[2], aPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(aArray, std::size(aArray));
			}

			if(i != 0)
			{
				IGraphics::CLineItem aArray[4] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aLastPoints[0], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aLastPoints[2], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aLastPoints[0], aLastPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[3], aLastPoints[2], aLastPoints[3])};
				Graphics()->LinesDraw(aArray, std::size(aArray));
			}

			if(i == NumSteps)
			{
				IGraphics::CLineItem aArray[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[0], aPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(aArray, std::size(aArray));
			}

			mem_copy(aLastPoints, aPoints, sizeof(aPoints));
		}

		// two screen sizes (green and red border)
		{
			Graphics()->SetColor(1, 0, 0, 1);
			for(int i = 0; i < 2; i++)
			{
				float aPoints[4];
				const float aAspects[] = {4.0f / 3.0f, 16.0f / 10.0f, 5.0f / 4.0f, 16.0f / 9.0f};
				float Aspect = aAspects[i];

				float Zoom = (m_ProofBorders == PROOF_BORDER_MENU) ? 0.7f : 1.0f;
				RenderTools()->MapScreenToWorld(
					WorldOffset.x, WorldOffset.y,
					100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, Zoom, aPoints);

				CUIRect r;
				r.x = aPoints[0];
				r.y = aPoints[1];
				r.w = aPoints[2] - aPoints[0];
				r.h = aPoints[3] - aPoints[1];

				IGraphics::CLineItem aArray[4] = {
					IGraphics::CLineItem(r.x, r.y, r.x + r.w, r.y),
					IGraphics::CLineItem(r.x + r.w, r.y, r.x + r.w, r.y + r.h),
					IGraphics::CLineItem(r.x + r.w, r.y + r.h, r.x, r.y + r.h),
					IGraphics::CLineItem(r.x, r.y + r.h, r.x, r.y)};
				Graphics()->LinesDraw(aArray, std::size(aArray));
				Graphics()->SetColor(0, 1, 0, 1);
			}
		}
		Graphics()->LinesEnd();

		// tee position (blue circle) and other screen positions
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 1, 0.3f);
			Graphics()->DrawCircle(WorldOffset.x, WorldOffset.y - 3.0f, 20.0f, 32);

			if(m_ProofBorders == PROOF_BORDER_MENU)
			{
				Graphics()->SetColor(0, 1, 0, 0.3f);

				std::set<int> Indices;
				for(int i = 0; i < (int)m_vMenuBackgroundPositions.size(); i++)
					Indices.insert(i);

				while(!Indices.empty())
				{
					int i = *Indices.begin();
					Indices.erase(i);
					for(int k : m_vMenuBackgroundCollisions.at(i))
						Indices.erase(k);

					vec2 Pos = m_vMenuBackgroundPositions[i];
					Pos += WorldOffset - m_vMenuBackgroundPositions[m_CurrentMenuProofIndex];

					if(Pos == WorldOffset)
						continue;

					Graphics()->DrawCircle(Pos.x, Pos.y - 3.0f, 20.0f, 32);
				}
			}

			Graphics()->QuadsEnd();
		}
	}
}

bool CProofMode::IsEnabled() const
{
	return m_ProofBorders != PROOF_BORDER_OFF;
}

bool CProofMode::IsModeMenu() const
{
	return m_ProofBorders == PROOF_BORDER_MENU;
}

bool CProofMode::IsModeIngame() const
{
	return m_ProofBorders == PROOF_BORDER_INGAME;
}

void CProofMode::Toggle()
{
	m_ProofBorders = m_ProofBorders == PROOF_BORDER_OFF ? PROOF_BORDER_INGAME : PROOF_BORDER_OFF;
}

void CProofMode::SetModeIngame()
{
	m_ProofBorders = PROOF_BORDER_INGAME;
}

void CProofMode::SetModeMenu()
{
	m_ProofBorders = PROOF_BORDER_MENU;
}
