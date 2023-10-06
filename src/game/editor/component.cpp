#include "component.h"

#include "editor.h"

void CEditorComponent::Init(CEditor *pEditor)
{
	m_pEditor = pEditor;
	OnReset();
}

void CEditorComponent::OnUpdate(CUIRect View)
{
	OnRender(View);
	if(IsActive())
		OnActive();
	else if(IsHot())
		OnHot();
}

bool CEditorComponent::OnInput(const IInput::CEvent &Event)
{
	for(CEditorComponent &Component : m_vSubComponents)
	{
		if(Component.OnInput(Event))
			return true;
	}
	return false;
}

void CEditorComponent::OnRender(CUIRect View) {}

void CEditorComponent::OnReset() {}
void CEditorComponent::OnMapLoad() {}

bool CEditorComponent::IsHot()
{
	return UI()->HotItem() == this;
}

void CEditorComponent::SetHot()
{
	UI()->SetHotItem(this);
}

void CEditorComponent::UnsetHot()
{
	if(IsHot())
		UI()->SetHotItem(nullptr);
}

void CEditorComponent::OnHot() {}

bool CEditorComponent::IsActive()
{
	return UI()->CheckActiveItem(this);
}

void CEditorComponent::SetActive()
{
	SetHot();
	UI()->SetActiveItem(this);
}

void CEditorComponent::SetInactive()
{
	if(IsActive())
		UI()->SetActiveItem(nullptr);
}

void CEditorComponent::OnActive() {}

void CEditorComponent::InitSubComponents()
{
	for(CEditorComponent &Component : m_vSubComponents)
	{
		Component.Init(Editor());
	}
}

void CEditorComponent::RegisterSubComponent(CEditorComponent &Component)
{
	m_vSubComponents.emplace_back(Component);
}

CEditor *CEditorComponent::Editor() { return m_pEditor; }
const CEditor *CEditorComponent::Editor() const { return m_pEditor; }
IInput *CEditorComponent::Input() { return m_pEditor->Input(); }
IClient *CEditorComponent::Client() { return m_pEditor->Client(); }
CConfig *CEditorComponent::Config() { return m_pEditor->Config(); }
IConsole *CEditorComponent::Console() { return m_pEditor->Console(); }
IEngine *CEditorComponent::Engine() { return m_pEditor->Engine(); }
IGraphics *CEditorComponent::Graphics() { return m_pEditor->Graphics(); }
ISound *CEditorComponent::Sound() { return m_pEditor->Sound(); }
ITextRender *CEditorComponent::TextRender() { return m_pEditor->TextRender(); }
IStorage *CEditorComponent::Storage() { return m_pEditor->Storage(); }
CUI *CEditorComponent::UI() { return m_pEditor->UI(); }
CRenderTools *CEditorComponent::RenderTools() { return m_pEditor->RenderTools(); }
