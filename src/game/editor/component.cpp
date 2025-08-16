#include "component.h"

void CEditorComponent::OnInit(CEditor *pEditor)
{
	CEditorObject::OnInit(pEditor);
	OnReset();
}

void CEditorComponent::OnReset()
{
}

void CEditorComponent::OnMapLoad()
{
}

bool CEditorComponent::OnInput(const IInput::CEvent &Event)
{
	for(CEditorComponent &Component : m_vSubComponents)
	{
		// Events with flag `FLAG_RELEASE` must always be forwarded to all components so keys being
		// released can be handled in all components also after some components have been disabled.
		if(Component.OnInput(Event) && (Event.m_Flags & ~IInput::FLAG_RELEASE) != 0)
		{
			return true;
		}
	}
	return false;
}

void CEditorComponent::OnUpdate()
{
}

void CEditorComponent::OnRender(CUIRect View)
{
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
