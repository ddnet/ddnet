#include "map_view.h"

#include "editor.h"

#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/ui.h>
#include <game/editor/editor_actions.h>
#include <game/editor/explanations.h>

void CMapView::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);
	RegisterSubComponent(m_MapGrid);
	RegisterSubComponent(m_ProofMode);
	InitSubComponents();
}

void CMapView::OnReset()
{
	m_Zoom = CSmoothValue(200.0f, 10.0f, 2000.0f);
	m_Zoom.OnInit(Editor());
	m_WorldZoom = 1.0f;

	SetWorldOffset({0, 0});
	SetEditorOffset({0, 0});

	m_ProofMode.OnReset();
	m_MapGrid.OnReset();
}

void CMapView::OnMapLoad()
{
	m_ProofMode.OnMapLoad();
}

bool CMapView::IsFocused()
{
	return GetWorldOffset() == (m_ProofMode.IsModeMenu() ? m_ProofMode.CurrentMenuBackgroundPosition() : vec2(0.0f, 0.0f));
}

void CMapView::Focus()
{
	SetWorldOffset(m_ProofMode.IsModeMenu() ? m_ProofMode.CurrentMenuBackgroundPosition() : vec2(0.0f, 0.0f));
}

void CMapView::RenderGroupBorder()
{
	std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();
	if(pGroup)
	{
		pGroup->MapScreen();

		for(size_t i = 0; i < Map()->m_vSelectedLayers.size(); i++)
		{
			std::shared_ptr<CLayer> pLayer = Map()->SelectedLayerType(i, LAYERTYPE_TILES);
			if(pLayer)
			{
				CUIRect BorderRect;
				BorderRect.x = 0.0f;
				BorderRect.y = 0.0f;
				pLayer->GetSize(&BorderRect.w, &BorderRect.h);
				BorderRect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			}
		}
	}
}

void CMapView::RenderEditorMap()
{
	if(Editor()->m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && Input()->KeyPress(KEY_G))
	{
		const bool AnyHidden =
			!Map()->m_pGameLayer->m_Visible ||
			(Map()->m_pFrontLayer && !Map()->m_pFrontLayer->m_Visible) ||
			(Map()->m_pTeleLayer && !Map()->m_pTeleLayer->m_Visible) ||
			(Map()->m_pSpeedupLayer && !Map()->m_pSpeedupLayer->m_Visible) ||
			(Map()->m_pTuneLayer && !Map()->m_pTuneLayer->m_Visible) ||
			(Map()->m_pSwitchLayer && !Map()->m_pSwitchLayer->m_Visible);
		Map()->m_pGameLayer->m_Visible = AnyHidden;
		if(Map()->m_pFrontLayer)
			Map()->m_pFrontLayer->m_Visible = AnyHidden;
		if(Map()->m_pTeleLayer)
			Map()->m_pTeleLayer->m_Visible = AnyHidden;
		if(Map()->m_pSpeedupLayer)
			Map()->m_pSpeedupLayer->m_Visible = AnyHidden;
		if(Map()->m_pTuneLayer)
			Map()->m_pTuneLayer->m_Visible = AnyHidden;
		if(Map()->m_pSwitchLayer)
			Map()->m_pSwitchLayer->m_Visible = AnyHidden;
	}

	for(auto &pGroup : Map()->m_vpGroups)
	{
		if(pGroup->m_Visible)
			pGroup->Render();
	}

	// render the game, tele, speedup, front, tune and switch above everything else
	if(Map()->m_pGameGroup->m_Visible)
	{
		Map()->m_pGameGroup->MapScreen();
		for(auto &pLayer : Map()->m_pGameGroup->m_vpLayers)
		{
			if(pLayer->m_Visible && pLayer->IsEntitiesLayer())
				pLayer->Render();
		}
	}

	std::shared_ptr<CLayerTiles> pSelectedTilesLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(Editor()->m_ShowTileInfo != CEditor::SHOW_TILE_OFF && pSelectedTilesLayer && pSelectedTilesLayer->m_Visible && m_Zoom.GetValue() <= 300.0f)
	{
		Map()->SelectedGroup()->MapScreen();
		pSelectedTilesLayer->ShowInfo();
	}
}

void CMapView::Render(CUIRect View)
{
	// render all good stuff
	if(!Editor()->m_ShowPicker)
	{
		RenderEditorMap();
	}
	else
	{
		// fix aspect ratio of the image in the picker
		float Max = minimum(View.w, View.h);
		View.w = View.h = Max;
	}

	const bool Inside = Ui()->MouseInside(&View);

	// fetch mouse position
	float wx = Ui()->MouseWorldX();
	float wy = Ui()->MouseWorldY();
	float mx = Ui()->MouseX();
	float my = Ui()->MouseY();

	static float s_StartWx = 0;
	static float s_StartWy = 0;

	// remap the screen so it can display the whole tileset
	if(Editor()->m_ShowPicker)
	{
		CUIRect Screen = *Ui()->Screen();
		float Size = 32.0f * 16.0f;
		float w = Size * (Screen.w / View.w);
		float h = Size * (Screen.h / View.h);
		float x = -(View.x / Screen.w) * w;
		float y = -(View.y / Screen.h) * h;
		wx = x + w * mx / Screen.w;
		wy = y + h * my / Screen.h;
		std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
		if(pTileLayer)
		{
			Graphics()->MapScreen(x, y, x + w, y + h);
			Editor()->m_pTilesetPicker->m_Image = pTileLayer->m_Image;
			if(Editor()->m_BrushColorEnabled)
			{
				Editor()->m_pTilesetPicker->m_Color = pTileLayer->m_Color;
				Editor()->m_pTilesetPicker->m_Color.a = 255;
			}
			else
			{
				Editor()->m_pTilesetPicker->m_Color = {255, 255, 255, 255};
			}

			Editor()->m_pTilesetPicker->m_HasGame = pTileLayer->m_HasGame;
			Editor()->m_pTilesetPicker->m_HasTele = pTileLayer->m_HasTele;
			Editor()->m_pTilesetPicker->m_HasSpeedup = pTileLayer->m_HasSpeedup;
			Editor()->m_pTilesetPicker->m_HasFront = pTileLayer->m_HasFront;
			Editor()->m_pTilesetPicker->m_HasSwitch = pTileLayer->m_HasSwitch;
			Editor()->m_pTilesetPicker->m_HasTune = pTileLayer->m_HasTune;

			Editor()->m_pTilesetPicker->Render(true);

			if(Editor()->m_ShowTileInfo != CEditor::SHOW_TILE_OFF)
				Editor()->m_pTilesetPicker->ShowInfo();
		}
		else
		{
			std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(Map()->SelectedLayerType(0, LAYERTYPE_QUADS));
			if(pQuadLayer)
			{
				Editor()->m_pQuadsetPicker->m_Image = pQuadLayer->m_Image;
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[0].x = f2fx(View.x);
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[0].y = f2fx(View.y);
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[1].x = f2fx((View.x + View.w));
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[1].y = f2fx(View.y);
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[2].x = f2fx(View.x);
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[2].y = f2fx((View.y + View.h));
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[3].x = f2fx((View.x + View.w));
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[3].y = f2fx((View.y + View.h));
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[4].x = f2fx((View.x + View.w / 2));
				Editor()->m_pQuadsetPicker->m_vQuads[0].m_aPoints[4].y = f2fx((View.y + View.h / 2));
				Editor()->m_pQuadsetPicker->Render();
			}
		}
	}

	// draw layer borders
	std::pair<int, std::shared_ptr<CLayer>> apEditLayers[128];
	size_t NumEditLayers = 0;

	if(Editor()->m_ShowPicker && Map()->SelectedLayer(0) && Map()->SelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		apEditLayers[0] = {0, Editor()->m_pTilesetPicker};
		NumEditLayers++;
	}
	else if(Editor()->m_ShowPicker)
	{
		apEditLayers[0] = {0, Editor()->m_pQuadsetPicker};
		NumEditLayers++;
	}
	else
	{
		// pick a type of layers to edit, preferring Tiles layers.
		int EditingType = -1;
		for(size_t i = 0; i < Map()->m_vSelectedLayers.size(); i++)
		{
			std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(i);
			if(pLayer && (EditingType == -1 || pLayer->m_Type == LAYERTYPE_TILES))
			{
				EditingType = pLayer->m_Type;
				if(EditingType == LAYERTYPE_TILES)
					break;
			}
		}
		for(size_t i = 0; i < Map()->m_vSelectedLayers.size() && NumEditLayers < 128; i++)
		{
			apEditLayers[NumEditLayers] = {Map()->m_vSelectedLayers[i], Map()->SelectedLayerType(i, EditingType)};
			if(apEditLayers[NumEditLayers].second)
			{
				NumEditLayers++;
			}
		}

		RenderGroupBorder();
		MapGrid()->Render();
	}

	const bool ShouldPan = Ui()->HotItem() == Editor()->MapView() && ((Input()->ModifierIsPressed() && Ui()->MouseButton(0)) || Ui()->MouseButton(2));
	if(Editor()->m_pContainerPanned == Editor()->MapView())
	{
		// do panning
		if(ShouldPan)
		{
			if(Input()->ShiftIsPressed())
				m_State = EState::PAN_EDITOR;
			else
				m_State = EState::PAN_WORLD;
			Ui()->SetActiveItem(Editor()->MapView());
		}
		else
			m_State = EState::NONE;

		if(m_State == EState::PAN_WORLD)
			OffsetWorld(-Ui()->MouseDelta() * MouseWorldScale());
		else if(m_State == EState::PAN_EDITOR)
			OffsetEditor(-Ui()->MouseDelta() * MouseWorldScale());

		if(m_State == EState::NONE)
			Editor()->m_pContainerPanned = nullptr;
	}

	if(Inside)
	{
		Ui()->SetHotItem(Editor()->MapView());

		// do global operations like pan and zoom
		if(Ui()->CheckActiveItem(nullptr) && (Ui()->MouseButton(0) || Ui()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;

			if(ShouldPan && Editor()->m_pContainerPanned == nullptr)
				Editor()->m_pContainerPanned = Editor()->MapView();
		}

		// brush editing
		if(Ui()->HotItem() == Editor()->MapView())
		{
			if(Editor()->m_ShowPicker)
			{
				std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
				int Layer;
				if(pLayer == Map()->m_pGameLayer)
					Layer = LAYER_GAME;
				else if(pLayer == Map()->m_pFrontLayer)
					Layer = LAYER_FRONT;
				else if(pLayer == Map()->m_pSwitchLayer)
					Layer = LAYER_SWITCH;
				else if(pLayer == Map()->m_pTeleLayer)
					Layer = LAYER_TELE;
				else if(pLayer == Map()->m_pSpeedupLayer)
					Layer = LAYER_SPEEDUP;
				else if(pLayer == Map()->m_pTuneLayer)
					Layer = LAYER_TUNE;
				else
					Layer = NUM_LAYERS;

				CExplanations::EGametype ExplanationGametype;
				if(Editor()->m_SelectEntitiesImage == "DDNet")
					ExplanationGametype = CExplanations::EGametype::DDNET;
				else if(Editor()->m_SelectEntitiesImage == "FNG")
					ExplanationGametype = CExplanations::EGametype::FNG;
				else if(Editor()->m_SelectEntitiesImage == "Race")
					ExplanationGametype = CExplanations::EGametype::RACE;
				else if(Editor()->m_SelectEntitiesImage == "Vanilla")
					ExplanationGametype = CExplanations::EGametype::VANILLA;
				else if(Editor()->m_SelectEntitiesImage == "blockworlds")
					ExplanationGametype = CExplanations::EGametype::BLOCKWORLDS;
				else
					ExplanationGametype = CExplanations::EGametype::NONE;

				if(Layer != NUM_LAYERS)
				{
					const char *pExplanation = CExplanations::Explain(ExplanationGametype, (int)wx / 32 + (int)wy / 32 * 16, Layer);
					if(pExplanation)
						str_copy(Editor()->m_aTooltip, pExplanation);
				}
			}
			else if(Editor()->m_pBrush->IsEmpty() && Map()->SelectedLayerType(0, LAYERTYPE_QUADS) != nullptr)
				str_copy(Editor()->m_aTooltip, "Use left mouse button to drag and create a brush. Hold shift to select multiple quads. Press R to rotate selected quads. Use ctrl+right click to select layer.");
			else if(Editor()->m_pBrush->IsEmpty())
			{
				if(g_Config.m_EdLayerSelector)
					str_copy(Editor()->m_aTooltip, "Use left mouse button to drag and create a brush. Use ctrl+right click to select layer of hovered tile.");
				else
					str_copy(Editor()->m_aTooltip, "Use left mouse button to drag and create a brush.");
			}
			else
			{
				// Alt behavior handled in CEditor::MouseAxisLock
				str_copy(Editor()->m_aTooltip, "Use left mouse button to paint with the brush. Right click to clear the brush. Hold Alt to lock the mouse movement to a single axis.");
			}

			if(Ui()->CheckActiveItem(Editor()->MapView()))
			{
				CUIRect r;
				r.x = s_StartWx;
				r.y = s_StartWy;
				r.w = wx - s_StartWx;
				r.h = wy - s_StartWy;
				if(r.w < 0)
				{
					r.x += r.w;
					r.w = -r.w;
				}

				if(r.h < 0)
				{
					r.y += r.h;
					r.h = -r.h;
				}

				if(m_State == EState::BRUSH_DRAW)
				{
					if(!Editor()->m_pBrush->IsEmpty())
					{
						// draw with brush
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k % Editor()->m_pBrush->m_vpLayers.size();
							if(apEditLayers[k].second->m_Type == Editor()->m_pBrush->m_vpLayers[BrushIndex]->m_Type)
							{
								if(apEditLayers[k].second->m_Type == LAYERTYPE_TILES)
								{
									std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(apEditLayers[k].second);
									std::shared_ptr<CLayerTiles> pBrushLayer = std::static_pointer_cast<CLayerTiles>(Editor()->m_pBrush->m_vpLayers[BrushIndex]);

									if((!pLayer->m_HasTele || pBrushLayer->m_HasTele) && (!pLayer->m_HasSpeedup || pBrushLayer->m_HasSpeedup) && (!pLayer->m_HasFront || pBrushLayer->m_HasFront) && (!pLayer->m_HasGame || pBrushLayer->m_HasGame) && (!pLayer->m_HasSwitch || pBrushLayer->m_HasSwitch) && (!pLayer->m_HasTune || pBrushLayer->m_HasTune))
										pLayer->BrushDraw(pBrushLayer.get(), vec2(wx, wy));
								}
								else
								{
									apEditLayers[k].second->BrushDraw(Editor()->m_pBrush->m_vpLayers[BrushIndex].get(), vec2(wx, wy));
								}
							}
						}
					}
				}
				else if(m_State == EState::BRUSH_GRAB)
				{
					if(!Ui()->MouseButton(0))
					{
						std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(Map()->SelectedLayerType(0, LAYERTYPE_QUADS));
						if(Input()->ShiftIsPressed() && pQuadLayer)
						{
							Map()->DeselectQuads();
							for(size_t i = 0; i < pQuadLayer->m_vQuads.size(); i++)
							{
								const CQuad &Quad = pQuadLayer->m_vQuads[i];
								vec2 Position = vec2(fx2f(Quad.m_aPoints[4].x), fx2f(Quad.m_aPoints[4].y));
								if(r.Inside(Position) && !Map()->IsQuadSelected(i))
									Map()->ToggleSelectQuad(i);
							}
						}
						else
						{
							// TODO: do all layers
							int Grabs = 0;
							for(size_t k = 0; k < NumEditLayers; k++)
								Grabs += apEditLayers[k].second->BrushGrab(Editor()->m_pBrush.get(), r);
							if(Grabs == 0)
								Editor()->m_pBrush->Clear();

							Map()->DeselectQuads();
							Map()->DeselectQuadPoints();
						}
					}
					else
					{
						if(NumEditLayers > 0)
						{
							apEditLayers[0].second->BrushSelecting(r);
						}
						Ui()->MapScreen();
					}
				}
				else if(m_State == EState::BRUSH_PAINT)
				{
					if(!Ui()->MouseButton(0))
					{
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(Editor()->m_pBrush->m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;
							std::shared_ptr<CLayer> pBrush = Editor()->m_pBrush->IsEmpty() ? nullptr : Editor()->m_pBrush->m_vpLayers[BrushIndex];
							apEditLayers[k].second->FillSelection(Editor()->m_pBrush->IsEmpty(), pBrush.get(), r);
						}
						std::shared_ptr<IEditorAction> Action = std::make_shared<CEditorBrushDrawAction>(Map(), Map()->m_SelectedGroup);
						Map()->m_EditorHistory.RecordAction(Action);
					}
					else
					{
						if(NumEditLayers > 0)
						{
							apEditLayers[0].second->BrushSelecting(r);
						}
						Ui()->MapScreen();
					}
				}
			}
			else
			{
				if(Ui()->MouseButton(1))
				{
					Editor()->m_pBrush->Clear();
				}

				if(!Input()->ModifierIsPressed() && Ui()->MouseButton(0) && m_State == EState::NONE && !Editor()->QuadKnife()->IsActive())
				{
					Ui()->SetActiveItem(Editor()->MapView());

					if(Editor()->m_pBrush->IsEmpty())
						m_State = EState::BRUSH_GRAB;
					else
					{
						m_State = EState::BRUSH_DRAW;
						for(size_t k = 0; k < NumEditLayers; k++)
						{
							size_t BrushIndex = k;
							if(Editor()->m_pBrush->m_vpLayers.size() != NumEditLayers)
								BrushIndex = 0;

							if(apEditLayers[k].second->m_Type == Editor()->m_pBrush->m_vpLayers[BrushIndex]->m_Type)
								apEditLayers[k].second->BrushPlace(Editor()->m_pBrush->m_vpLayers[BrushIndex].get(), vec2(wx, wy));
						}
					}

					std::shared_ptr<CLayerTiles> pLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
					if(Input()->ShiftIsPressed() && pLayer)
						m_State = EState::BRUSH_PAINT;
				}

				if(!Editor()->m_pBrush->IsEmpty())
				{
					Editor()->m_pBrush->m_OffsetX = -(int)wx;
					Editor()->m_pBrush->m_OffsetY = -(int)wy;
					for(const auto &pLayer : Editor()->m_pBrush->m_vpLayers)
					{
						if(pLayer->m_Type == LAYERTYPE_TILES)
						{
							Editor()->m_pBrush->m_OffsetX = -(int)(wx / 32.0f) * 32;
							Editor()->m_pBrush->m_OffsetY = -(int)(wy / 32.0f) * 32;
							break;
						}
					}

					std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();
					if(!Editor()->m_ShowPicker && pGroup)
					{
						Editor()->m_pBrush->m_OffsetX += pGroup->m_OffsetX;
						Editor()->m_pBrush->m_OffsetY += pGroup->m_OffsetY;
						Editor()->m_pBrush->m_ParallaxX = pGroup->m_ParallaxX;
						Editor()->m_pBrush->m_ParallaxY = pGroup->m_ParallaxY;
						Editor()->m_pBrush->Render();

						CUIRect BorderRect;
						BorderRect.x = 0.0f;
						BorderRect.y = 0.0f;
						Editor()->m_pBrush->GetSize(&BorderRect.w, &BorderRect.h);
						BorderRect.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
					}
				}
			}
		}

		// quad & sound editing
		{
			if(!Editor()->m_ShowPicker && Editor()->m_pBrush->IsEmpty())
			{
				// fetch layers
				std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();
				if(pGroup)
					pGroup->MapScreen();

				for(size_t k = 0; k < NumEditLayers; k++)
				{
					auto &[LayerIndex, pEditLayer] = apEditLayers[k];

					if(pEditLayer->m_Type == LAYERTYPE_QUADS)
					{
						std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(pEditLayer);

						if(Editor()->m_ActiveEnvelopePreview == CEditor::EEnvelopePreview::NONE)
							Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::ALL;

						if(Editor()->QuadKnife()->IsActive())
						{
							Editor()->QuadKnife()->DoSlice();
						}
						else
						{
							Editor()->UpdateHotQuadPoint(pLayer.get());

							Graphics()->TextureClear();
							Graphics()->QuadsBegin();
							for(size_t i = 0; i < pLayer->m_vQuads.size(); i++)
							{
								for(int v = 0; v < 4; v++)
									Editor()->DoQuadPoint(LayerIndex, pLayer, &pLayer->m_vQuads[i], i, v);

								Editor()->DoQuad(LayerIndex, pLayer, &pLayer->m_vQuads[i], i);
							}
							Graphics()->QuadsEnd();
						}
					}
					else if(pEditLayer->m_Type == LAYERTYPE_SOUNDS)
					{
						std::shared_ptr<CLayerSounds> pLayer = std::static_pointer_cast<CLayerSounds>(pEditLayer);

						Editor()->UpdateHotSoundSource(pLayer.get());

						Graphics()->TextureClear();
						Graphics()->QuadsBegin();
						for(size_t i = 0; i < pLayer->m_vSources.size(); i++)
						{
							Editor()->DoSoundSource(LayerIndex, &pLayer->m_vSources[i], i);
						}
						Graphics()->QuadsEnd();
					}
				}

				Ui()->MapScreen();
			}
		}

		// menu proof selection
		if(ProofMode()->IsModeMenu() && !Editor()->m_ShowPicker)
		{
			ProofMode()->InitMenuBackgroundPositions();
			const std::vector<vec2> &MenuBackgroundPositions = ProofMode()->MenuBackgroundPositions();
			for(int i = 0; i < (int)MenuBackgroundPositions.size(); i++)
			{
				vec2 Pos = MenuBackgroundPositions[i];
				const void *pId = &MenuBackgroundPositions[i];
				Pos += GetWorldOffset() - MenuBackgroundPositions[ProofMode()->CurrentMenuProofIndex()];
				Pos.y -= 3.0f;

				if(distance(Pos, m_MouseWorldNoParaPos) <= 20.0f)
				{
					Ui()->SetHotItem(pId);

					if(i != ProofMode()->CurrentMenuProofIndex() && Ui()->CheckActiveItem(pId))
					{
						if(!Ui()->MouseButton(0))
						{
							ProofMode()->SetCurrentMenuProofIndex(i);
							SetWorldOffset(MenuBackgroundPositions[i]);
							Ui()->SetActiveItem(nullptr);
						}
					}
					else if(Ui()->HotItem() == pId)
					{
						char aTooltipPrefix[32] = "Switch proof position to";
						if(i == ProofMode()->CurrentMenuProofIndex())
							str_copy(aTooltipPrefix, "Current proof position at");

						char aNumBuf[8];
						if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
							str_format(aNumBuf, sizeof(aNumBuf), "#%d", i + 1);
						else
							aNumBuf[0] = '\0';

						char aTooltipPositions[128];
						str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s %s", ProofMode()->MenuBackgroundPositionName(i), aNumBuf);

						for(int k : ProofMode()->MenuBackgroundCollisions(i))
						{
							if(k == ProofMode()->CurrentMenuProofIndex())
								str_copy(aTooltipPrefix, "Current proof position at");

							Pos = MenuBackgroundPositions[k];
							Pos += GetWorldOffset() - MenuBackgroundPositions[ProofMode()->CurrentMenuProofIndex()];
							Pos.y -= 3.0f;

							if(distance(Pos, m_MouseWorldNoParaPos) > 20.0f)
								continue;

							if(i < (TILE_TIME_CHECKPOINT_LAST - TILE_TIME_CHECKPOINT_FIRST))
								str_format(aNumBuf, sizeof(aNumBuf), "#%d", k + 1);
							else
								aNumBuf[0] = '\0';

							char aTooltipPositionsCopy[128];
							str_copy(aTooltipPositionsCopy, aTooltipPositions);
							str_format(aTooltipPositions, sizeof(aTooltipPositions), "%s, %s %s", aTooltipPositionsCopy, ProofMode()->MenuBackgroundPositionName(k), aNumBuf);
						}
						str_format(Editor()->m_aTooltip, sizeof(Editor()->m_aTooltip), "%s %s.", aTooltipPrefix, aTooltipPositions);

						if(Ui()->MouseButton(0))
							Ui()->SetActiveItem(pId);
					}
					break;
				}
			}
		}

		if(!Input()->ModifierIsPressed() && Editor()->m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr)
		{
			float PanSpeed = Input()->ShiftIsPressed() ? 200.0f : 64.0f;
			if(Input()->KeyPress(KEY_A))
				OffsetWorld({-PanSpeed * MouseWorldScale(), 0});
			else if(Input()->KeyPress(KEY_D))
				OffsetWorld({PanSpeed * MouseWorldScale(), 0});
			if(Input()->KeyPress(KEY_W))
				OffsetWorld({0, -PanSpeed * MouseWorldScale()});
			else if(Input()->KeyPress(KEY_S))
				OffsetWorld({0, PanSpeed * MouseWorldScale()});
		}
	}

	if(Ui()->CheckActiveItem(Editor()->MapView()) && Editor()->m_pContainerPanned == nullptr)
	{
		// release mouse
		if(!Ui()->MouseButton(0))
		{
			if(m_State == EState::BRUSH_DRAW)
			{
				std::shared_ptr<IEditorAction> pAction = std::make_shared<CEditorBrushDrawAction>(Map(), Map()->m_SelectedGroup);

				if(!pAction->IsEmpty()) // Avoid recording tile draw action when placing quads only
					Map()->m_EditorHistory.RecordAction(pAction);
			}

			m_State = EState::NONE;
			Ui()->SetActiveItem(nullptr);
		}
	}

	if(!Editor()->m_ShowPicker && Map()->SelectedGroup() && Map()->SelectedGroup()->m_UseClipping)
	{
		std::shared_ptr<CLayerGroup> pGameGroup = Map()->m_pGameGroup;
		pGameGroup->MapScreen();

		CUIRect ClipRect;
		ClipRect.x = Map()->SelectedGroup()->m_ClipX;
		ClipRect.y = Map()->SelectedGroup()->m_ClipY;
		ClipRect.w = Map()->SelectedGroup()->m_ClipW;
		ClipRect.h = Map()->SelectedGroup()->m_ClipH;
		ClipRect.DrawOutline(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
	}

	if(!Editor()->m_ShowPicker)
		ProofMode()->RenderScreenSizes();

	if(!Editor()->m_ShowPicker && Editor()->m_ShowEnvelopePreview && Editor()->m_ActiveEnvelopePreview != CEditor::EEnvelopePreview::NONE)
	{
		const std::shared_ptr<CLayer> pSelectedLayer = Map()->SelectedLayer(0);
		if(pSelectedLayer != nullptr && pSelectedLayer->m_Type == LAYERTYPE_QUADS)
		{
			Editor()->DoQuadEnvelopes(static_cast<const CLayerQuads *>(pSelectedLayer.get()));
		}
		Editor()->m_ActiveEnvelopePreview = CEditor::EEnvelopePreview::NONE;
	}

	Ui()->MapScreen();
}

void CMapView::UpdateMouseWorld()
{
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
	const vec2 UpdatedMouseDelta = Ui()->UpdatedMouseDelta();

	// fix correct world x and y
	const std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();
	if(pGroup)
	{
		float aPoints[4];
		pGroup->Mapping(aPoints);

		float WorldWidth = aPoints[2] - aPoints[0];
		float WorldHeight = aPoints[3] - aPoints[1];

		m_MouseWorldScale = WorldWidth / Graphics()->WindowWidth();

		m_MouseWorldPos.x = aPoints[0] + WorldWidth * (UpdatedMousePos.x / Graphics()->WindowWidth());
		m_MouseWorldPos.y = aPoints[1] + WorldHeight * (UpdatedMousePos.y / Graphics()->WindowHeight());
		m_MouseDeltaWorld.x = UpdatedMouseDelta.x * (WorldWidth / Graphics()->WindowWidth());
		m_MouseDeltaWorld.y = UpdatedMouseDelta.y * (WorldHeight / Graphics()->WindowHeight());
	}
	else
	{
		m_MouseWorldPos = vec2(-1.0f, -1.0f);
		m_MouseDeltaWorld = vec2(0.0f, 0.0f);
	}

	m_MouseWorldNoParaPos = vec2(-1.0f, -1.0f);
	for(const std::shared_ptr<CLayerGroup> &pGameGroup : Map()->m_vpGroups)
	{
		if(!pGameGroup->m_GameGroup)
			continue;

		float aPoints[4];
		pGameGroup->Mapping(aPoints);

		float WorldWidth = aPoints[2] - aPoints[0];
		float WorldHeight = aPoints[3] - aPoints[1];

		m_MouseWorldNoParaPos.x = aPoints[0] + WorldWidth * (UpdatedMousePos.x / Graphics()->WindowWidth());
		m_MouseWorldNoParaPos.y = aPoints[1] + WorldHeight * (UpdatedMousePos.y / Graphics()->WindowHeight());
	}

	Editor()->OnMouseMove(UpdatedMousePos);
}

void CMapView::ResetMouseDeltaWorld()
{
	m_MouseDeltaWorld = vec2(0.0f, 0.0f);
}

float CMapView::MouseWorldScale() const
{
	return m_MouseWorldScale;
}

vec2 CMapView::MouseDeltaWorld() const
{
	return m_MouseDeltaWorld;
}

float CMapView::MouseDeltaWorldX() const
{
	return m_MouseDeltaWorld.x;
}

float CMapView::MouseDeltaWorldY() const
{
	return m_MouseDeltaWorld.y;
}

vec2 CMapView::MouseWorldPos() const
{
	return m_MouseWorldPos;
}

float CMapView::MouseWorldX() const
{
	return m_MouseWorldPos.x;
}

float CMapView::MouseWorldY() const
{
	return m_MouseWorldPos.y;
}

void CMapView::ResetZoom()
{
	SetEditorOffset({0, 0});
	m_Zoom.SetValue(100.0f);
}

float CMapView::ScaleLength(float Value) const
{
	return m_WorldZoom * Value;
}

void CMapView::ZoomMouseTarget(float ZoomFactor)
{
	// zoom to the current mouse position
	// get absolute mouse position
	float aPoints[4];
	Graphics()->MapScreenToWorld(
		GetWorldOffset().x, GetWorldOffset().y,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2] - aPoints[0];
	float WorldHeight = aPoints[3] - aPoints[1];

	float MouseWorldX = aPoints[0] + WorldWidth * (Ui()->MouseX() / Ui()->Screen()->w);
	float MouseWorldY = aPoints[1] + WorldHeight * (Ui()->MouseY() / Ui()->Screen()->h);

	// adjust camera
	OffsetWorld((vec2(MouseWorldX, MouseWorldY) - GetWorldOffset()) * (1.0f - ZoomFactor));
}

void CMapView::UpdateZoom()
{
	float OldLevel = m_Zoom.GetValue();
	bool UpdatedZoom = m_Zoom.UpdateValue();
	m_Zoom.SetValueRange(10.0f, g_Config.m_EdLimitMaxZoomLevel ? 2000.0f : std::numeric_limits<float>::max());
	float NewLevel = m_Zoom.GetValue();
	if(UpdatedZoom && g_Config.m_EdZoomTarget)
		ZoomMouseTarget(NewLevel / OldLevel);
	m_WorldZoom = NewLevel / 100.0f;
}

CSmoothValue *CMapView::Zoom()
{
	return &m_Zoom;
}

const CSmoothValue *CMapView::Zoom() const
{
	return &m_Zoom;
}

CProofMode *CMapView::ProofMode()
{
	return &m_ProofMode;
}

const CProofMode *CMapView::ProofMode() const
{
	return &m_ProofMode;
}

CMapGrid *CMapView::MapGrid()
{
	return &m_MapGrid;
}

const CMapGrid *CMapView::MapGrid() const
{
	return &m_MapGrid;
}

void CMapView::OffsetWorld(vec2 Offset)
{
	m_WorldOffset += Offset;
}

void CMapView::OffsetEditor(vec2 Offset)
{
	m_EditorOffset += Offset;
}

void CMapView::SetWorldOffset(vec2 WorldOffset)
{
	m_WorldOffset = WorldOffset;
}

void CMapView::SetEditorOffset(vec2 EditorOffset)
{
	m_EditorOffset = EditorOffset;
}

vec2 CMapView::GetWorldOffset() const
{
	return m_WorldOffset;
}

vec2 CMapView::GetEditorOffset() const
{
	return m_EditorOffset;
}

float CMapView::GetWorldZoom() const
{
	return m_WorldZoom;
}
