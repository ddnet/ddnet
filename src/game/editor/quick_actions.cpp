#include "editor.h"
#include "editor_actions.h"

#include <engine/keys.h>

#include <game/client/gameclient.h>
#include <game/mapitems.h>

void CEditor::FillGameTiles(EGameTileOp FillTile) const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		pTileLayer->FillGameTiles(FillTile);
}

bool CEditor::CanFillGameTiles() const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		return pTileLayer->CanFillGameTiles();
	return false;
}

void CEditor::AddQuadOrSound()
{
	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	if(!pLayer)
		return;
	if(pLayer->m_Type != LAYERTYPE_QUADS && pLayer->m_Type != LAYERTYPE_SOUNDS)
		return;

	std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();

	float aMapping[4];
	pGroup->Mapping(aMapping);
	int x = aMapping[0] + (aMapping[2] - aMapping[0]) / 2;
	int y = aMapping[1] + (aMapping[3] - aMapping[1]) / 2;
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && Input()->ModifierIsPressed())
	{
		x += Ui()->MouseWorldX() - (MapView()->GetWorldOffset().x * pGroup->m_ParallaxX / 100) - pGroup->m_OffsetX;
		y += Ui()->MouseWorldY() - (MapView()->GetWorldOffset().y * pGroup->m_ParallaxY / 100) - pGroup->m_OffsetY;
	}

	if(pLayer->m_Type == LAYERTYPE_QUADS)
		Map()->m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptyQuad>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0], x, y));
	else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
		Map()->m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptySound>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0], x, y));
}

void CEditor::AddGroup()
{
	Map()->NewGroup();
	Map()->m_SelectedGroup = Map()->m_vpGroups.size() - 1;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(Map(), Map()->m_SelectedGroup, false));
}

void CEditor::AddSoundLayer()
{
	std::shared_ptr<CLayer> pSoundLayer = std::make_shared<CLayerSounds>(Map());
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSoundLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTileLayer()
{
	std::shared_ptr<CLayer> pTileLayer = std::make_shared<CLayerTiles>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTileLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddQuadsLayer()
{
	std::shared_ptr<CLayer> pQuadLayer = std::make_shared<CLayerQuads>(Map());
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pQuadLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddSwitchLayer()
{
	std::shared_ptr<CLayer> pSwitchLayer = std::make_shared<CLayerSwitch>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeSwitchLayer(pSwitchLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSwitchLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddFrontLayer()
{
	std::shared_ptr<CLayer> pFrontLayer = std::make_shared<CLayerFront>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeFrontLayer(pFrontLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pFrontLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTuneLayer()
{
	std::shared_ptr<CLayer> pTuneLayer = std::make_shared<CLayerTune>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeTuneLayer(pTuneLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTuneLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddSpeedupLayer()
{
	std::shared_ptr<CLayer> pSpeedupLayer = std::make_shared<CLayerSpeedup>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeSpeedupLayer(pSpeedupLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSpeedupLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTeleLayer()
{
	std::shared_ptr<CLayer> pTeleLayer = std::make_shared<CLayerTele>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeTeleLayer(pTeleLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTeleLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

bool CEditor::IsNonGameTileLayerSelected() const
{
	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	if(!pLayer)
		return false;
	if(pLayer->m_Type != LAYERTYPE_TILES)
		return false;
	if(
		pLayer == Map()->m_pGameLayer ||
		pLayer == Map()->m_pFrontLayer ||
		pLayer == Map()->m_pSwitchLayer ||
		pLayer == Map()->m_pTeleLayer ||
		pLayer == Map()->m_pSpeedupLayer ||
		pLayer == Map()->m_pTuneLayer)
		return false;

	return true;
}

void CEditor::LayerSelectImage()
{
	if(!IsNonGameTileLayerSelected())
		return;

	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

	static SLayerPopupContext s_LayerPopupContext = {};
	s_LayerPopupContext.m_pEditor = this;
	Ui()->DoPopupMenu(&s_LayerPopupContext, Ui()->MouseX(), Ui()->MouseY(), 150, 300, &s_LayerPopupContext, PopupLayer);
	PopupSelectImageInvoke(pTiles->m_Image, Ui()->MouseX(), Ui()->MouseY());
}

void CEditor::MapDetails()
{
	const CUIRect *pScreen = Ui()->Screen();
	Map()->m_MapInfoTmp.Copy(Map()->m_MapInfo);
	static SPopupMenuId s_PopupMapInfoId;
	constexpr float PopupWidth = 400.0f;
	constexpr float PopupHeight = 170.0f;
	Ui()->DoPopupMenu(
		&s_PopupMapInfoId,
		pScreen->w / 2.0f - PopupWidth / 2.0f,
		pScreen->h / 2.0f - PopupHeight / 2.0f,
		PopupWidth,
		PopupHeight,
		this,
		PopupMapInfo);
	Ui()->SetActiveItem(nullptr);
}

void CEditor::DeleteSelectedLayer()
{
	std::shared_ptr<CLayer> pCurrentLayer = Map()->SelectedLayer(0);
	if(!pCurrentLayer)
		return;
	if(Map()->m_pGameLayer == pCurrentLayer)
		return;

	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionDeleteLayer>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0]));

	if(pCurrentLayer == Map()->m_pFrontLayer)
		Map()->m_pFrontLayer = nullptr;
	if(pCurrentLayer == Map()->m_pTeleLayer)
		Map()->m_pTeleLayer = nullptr;
	if(pCurrentLayer == Map()->m_pSpeedupLayer)
		Map()->m_pSpeedupLayer = nullptr;
	if(pCurrentLayer == Map()->m_pSwitchLayer)
		Map()->m_pSwitchLayer = nullptr;
	if(pCurrentLayer == Map()->m_pTuneLayer)
		Map()->m_pTuneLayer = nullptr;
	Map()->m_vpGroups[Map()->m_SelectedGroup]->DeleteLayer(Map()->m_vSelectedLayers[0]);

	Map()->SelectPreviousLayer();
}

void CEditor::TestMapLocally()
{
	const char *pFilenameNoMaps = str_startswith(Map()->m_aFilename, "maps/");
	if(!pFilenameNoMaps)
	{
		ShowFileDialogError("The map isn't saved in the maps/ folder. It must be saved there to load on the server.");
		return;
	}

	char aFilenameNoExt[IO_MAX_PATH_LENGTH];
	fs_split_file_extension(pFilenameNoMaps, aFilenameNoExt, sizeof(aFilenameNoExt));

	if(Client()->RconAuthed())
	{
		if(net_addr_is_local(&Client()->ServerAddress()))
		{
			OnClose();
			g_Config.m_ClEditor = 0;
			char aMapChange[IO_MAX_PATH_LENGTH + 64];
			str_format(aMapChange, sizeof(aMapChange), "change_map %s", aFilenameNoExt);
			Client()->Rcon(aMapChange);
			return;
		}
	}

	CGameClient *pGameClient = (CGameClient *)Kernel()->RequestInterface<IGameClient>();
	if(pGameClient->m_LocalServer.IsServerRunning())
	{
		m_PopupEventType = CEditor::POPEVENT_RESTART_SERVER;
		m_PopupEventActivated = true;
	}
	else
	{
		char aMapChange[IO_MAX_PATH_LENGTH + 64];
		str_format(aMapChange, sizeof(aMapChange), "change_map %s", aFilenameNoExt);
		pGameClient->m_LocalServer.RunServer({"sv_register 0", aMapChange});
		OnClose();
		g_Config.m_ClEditor = 0;
		Client()->Connect("localhost");
	}
}
