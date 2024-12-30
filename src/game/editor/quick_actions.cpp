#include <engine/keys.h>
#include <game/client/gameclient.h>
#include <game/mapitems.h>

#include "editor.h"

#include "editor_actions.h"

void CEditor::FillGameTiles(EGameTileOp FillTile) const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		pTileLayer->FillGameTiles(FillTile);
}

bool CEditor::CanFillGameTiles() const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		return pTileLayer->CanFillGameTiles();
	return false;
}

void CEditor::AddQuadOrSound()
{
	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
	if(!pLayer)
		return;
	if(pLayer->m_Type != LAYERTYPE_QUADS && pLayer->m_Type != LAYERTYPE_SOUNDS)
		return;

	std::shared_ptr<CLayerGroup> pGroup = GetSelectedGroup();

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
		m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptyQuad>(this, m_SelectedGroup, m_vSelectedLayers[0], x, y));
	else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
		m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptySound>(this, m_SelectedGroup, m_vSelectedLayers[0], x, y));
}

void CEditor::AddGroup()
{
	m_Map.NewGroup();
	m_SelectedGroup = m_Map.m_vpGroups.size() - 1;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(this, m_SelectedGroup, false));
}

void CEditor::AddSoundLayer()
{
	std::shared_ptr<CLayer> pSoundLayer = std::make_shared<CLayerSounds>(this);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pSoundLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse = false;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddTileLayer()
{
	std::shared_ptr<CLayer> pTileLayer = std::make_shared<CLayerTiles>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	pTileLayer->m_pEditor = this;
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pTileLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse = false;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddQuadsLayer()
{
	std::shared_ptr<CLayer> pQuadLayer = std::make_shared<CLayerQuads>(this);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pQuadLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse = false;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddSwitchLayer()
{
	std::shared_ptr<CLayer> pSwitchLayer = std::make_shared<CLayerSwitch>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	m_Map.MakeSwitchLayer(pSwitchLayer);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pSwitchLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_pBrush->Clear();
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddFrontLayer()
{
	std::shared_ptr<CLayer> pFrontLayer = std::make_shared<CLayerFront>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	m_Map.MakeFrontLayer(pFrontLayer);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pFrontLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_pBrush->Clear();
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddTuneLayer()
{
	std::shared_ptr<CLayer> pTuneLayer = std::make_shared<CLayerTune>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	m_Map.MakeTuneLayer(pTuneLayer);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pTuneLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_pBrush->Clear();
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddSpeedupLayer()
{
	std::shared_ptr<CLayer> pSpeedupLayer = std::make_shared<CLayerSpeedup>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	m_Map.MakeSpeedupLayer(pSpeedupLayer);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pSpeedupLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_pBrush->Clear();
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

void CEditor::AddTeleLayer()
{
	std::shared_ptr<CLayer> pTeleLayer = std::make_shared<CLayerTele>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	m_Map.MakeTeleLayer(pTeleLayer);
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pTeleLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_pBrush->Clear();
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}

bool CEditor::IsNonGameTileLayerSelected() const
{
	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
	if(!pLayer)
		return false;
	if(pLayer->m_Type != LAYERTYPE_TILES)
		return false;
	if(
		pLayer == m_Map.m_pGameLayer ||
		pLayer == m_Map.m_pFrontLayer ||
		pLayer == m_Map.m_pSwitchLayer ||
		pLayer == m_Map.m_pTeleLayer ||
		pLayer == m_Map.m_pSpeedupLayer ||
		pLayer == m_Map.m_pTuneLayer)
		return false;

	return true;
}

void CEditor::LayerSelectImage()
{
	if(!IsNonGameTileLayerSelected())
		return;

	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
	std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

	static SLayerPopupContext s_LayerPopupContext = {};
	s_LayerPopupContext.m_pEditor = this;
	Ui()->DoPopupMenu(&s_LayerPopupContext, Ui()->MouseX(), Ui()->MouseY(), 150, 300, &s_LayerPopupContext, PopupLayer);
	PopupSelectImageInvoke(pTiles->m_Image, Ui()->MouseX(), Ui()->MouseY());
}

void CEditor::MapDetails()
{
	const CUIRect *pScreen = Ui()->Screen();
	m_Map.m_MapInfoTmp.Copy(m_Map.m_MapInfo);
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
	std::shared_ptr<CLayer> pCurrentLayer = GetSelectedLayer(0);
	if(!pCurrentLayer)
		return;
	if(m_Map.m_pGameLayer == pCurrentLayer)
		return;

	m_EditorHistory.RecordAction(std::make_shared<CEditorActionDeleteLayer>(this, m_SelectedGroup, m_vSelectedLayers[0]));

	if(pCurrentLayer == m_Map.m_pFrontLayer)
		m_Map.m_pFrontLayer = nullptr;
	if(pCurrentLayer == m_Map.m_pTeleLayer)
		m_Map.m_pTeleLayer = nullptr;
	if(pCurrentLayer == m_Map.m_pSpeedupLayer)
		m_Map.m_pSpeedupLayer = nullptr;
	if(pCurrentLayer == m_Map.m_pSwitchLayer)
		m_Map.m_pSwitchLayer = nullptr;
	if(pCurrentLayer == m_Map.m_pTuneLayer)
		m_Map.m_pTuneLayer = nullptr;
	m_Map.m_vpGroups[m_SelectedGroup]->DeleteLayer(m_vSelectedLayers[0]);

	SelectPreviousLayer();
}

void CEditor::TestMapLocally()
{
	const char *pFileNameNoMaps = str_startswith(m_aFileName, "maps/");
	if(!pFileNameNoMaps)
	{
		ShowFileDialogError("The map isn't saved in the maps/ folder. It must be saved there to load on the server.");
		return;
	}

	char aFileNameNoExt[IO_MAX_PATH_LENGTH];
	fs_split_file_extension(pFileNameNoMaps, aFileNameNoExt, sizeof(aFileNameNoExt));
	char aBuf[IO_MAX_PATH_LENGTH + 64];

	if(Client()->RconAuthed())
	{
		if(net_addr_is_local(&Client()->ServerAddress()))
		{
			OnClose();
			g_Config.m_ClEditor = 0;
			str_format(aBuf, sizeof(aBuf), "change_map %s", aFileNameNoExt);
			Client()->Rcon(aBuf);
			return;
		}
	}

	CGameClient *pGameClient = (CGameClient *)Kernel()->RequestInterface<IGameClient>();
	if(pGameClient->m_Menus.IsServerRunning())
	{
		m_PopupEventType = CEditor::POPEVENT_RESTART_SERVER;
		m_PopupEventActivated = true;
	}
	else
	{
		char aRegister[] = "sv_register 0";

		char aRandomPass[17];
		secure_random_password(aRandomPass, sizeof(aRandomPass), 16);
		char aPass[64];
		str_format(aPass, sizeof(aPass), "auth_add %s admin %s", DEFAULT_SAVED_RCON_USER, aRandomPass);
		str_copy(pGameClient->m_aSavedLocalRconPassword, aRandomPass);

		str_format(aBuf, sizeof(aBuf), "change_map %s", aFileNameNoExt);
		const char *apArguments[] = {aRegister, aPass, aBuf};
		pGameClient->m_Menus.RunServer(apArguments, std::size(apArguments));
		OnClose();
		g_Config.m_ClEditor = 0;
		Client()->Connect("localhost");
	}
}
