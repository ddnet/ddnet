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
}

void CEditorComponent::OnUpdate(CUIRect View)
{
	OnInput();
	OnRender(View);
}

void CEditorComponent::OnInput() {}
void CEditorComponent::OnRender(CUIRect View) {}

CEditor *CEditorComponent::Editor() { return m_pEditor; }
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
