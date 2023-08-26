#include "component.h"

#include "editor.h"

void CEditorComponent::Init(CEditor *pEditor)
{
	m_pEditor = pEditor;
	m_pInput = pEditor->Input();
	m_pClient = pEditor->Client();
	m_pConfig = pEditor->Config();
	m_pConsole = pEditor->Console();
	m_pEngine = pEditor->Engine();
	m_pGraphics = pEditor->Graphics();
	m_pSound = pEditor->Sound();
	m_pTextRender = pEditor->TextRender();
	m_pStorage = pEditor->Storage();
	m_pUI = pEditor->UI();
	m_pRenderTools = pEditor->RenderTools();

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
IInput *CEditorComponent::Input() { return m_pInput; }
IClient *CEditorComponent::Client() { return m_pClient; }
CConfig *CEditorComponent::Config() { return m_pConfig; }
IConsole *CEditorComponent::Console() { return m_pConsole; }
IEngine *CEditorComponent::Engine() { return m_pEngine; }
IGraphics *CEditorComponent::Graphics() { return m_pGraphics; }
ISound *CEditorComponent::Sound() { return m_pSound; }
ITextRender *CEditorComponent::TextRender() { return m_pTextRender; }
IStorage *CEditorComponent::Storage() { return m_pStorage; }
CUI *CEditorComponent::UI() { return m_pUI; }
CRenderTools *CEditorComponent::RenderTools() { return m_pRenderTools; }
