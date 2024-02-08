#include "editor.h"

#include "layer_selector.h"

void CLayerSelector::Init(CEditor *pEditor)
{
	CEditorComponent::Init(pEditor);

	m_SelectionOffset = 1;
}

bool CLayerSelector::SelectByTile()
{
	// ctrl+rightclick a map index to select the layer that has a tile there
	static bool s_CtrlClick = false;
	if(!UI()->CheckActiveItem(nullptr))
		return false;
	if(!UI()->MouseButton(1) || !Input()->ModifierIsPressed())
	{
		s_CtrlClick = false;
		return false;
	}

	if(s_CtrlClick)
		return false;
	s_CtrlClick = true;

	if(m_MatchedGroup == -1 || m_MatchedLayer == -1)
		return false;

	m_SelectionOffset++;
	if(!m_Found)
		m_SelectionOffset = 2;
	Editor()->SelectLayer(m_MatchedLayer, m_MatchedGroup);
	return true;
}

void CLayerSelector::BeforeHoverTile()
{
	m_MatchedGroup = -1;
	m_MatchedLayer = -1;
	m_Matches = 0;
	m_Found = false;
}

void CLayerSelector::OnHoverTile(int Group, int Layer, const CTile &Tile, int x, int y)
{
	if(m_Found)
		return;

	if(m_MatchedGroup == -1)
	{
		m_MatchedGroup = Group;
		m_MatchedLayer = Layer;
	}
	m_Matches++;
	if(m_Matches == m_SelectionOffset)
	{
		m_MatchedGroup = Group;
		m_MatchedLayer = Layer;
		m_Found = true;
	}
}
