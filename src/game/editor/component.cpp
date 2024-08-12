#include "component.h"

bool CEditorComponent::OnInput(const IInput::CEvent &Event)
{
	for(CEditorComponent &Component : m_vSubComponents)
	{
		if(Component.OnInput(Event))
			return true;
	}
	return false;
}

void CEditorComponent::InitSubComponents()
{
	for(CEditorComponent &Component : m_vSubComponents)
	{
		Component.OnInit(Editor());
	}
}

void CEditorComponent::RegisterSubComponent(CEditorComponent &Component)
{
	m_vSubComponents.emplace_back(Component);
}
