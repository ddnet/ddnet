#include "editor_object.h"

#include "editor.h"

void CEditorObject::Init(CEditor *pEditor)
{
	m_pEditor = pEditor;
	OnReset();
}

void CEditorObject::OnUpdate()
{
	if(IsActive())
		OnActive();
	else if(IsHot())
		OnHot();
}

bool CEditorObject::OnInput(const IInput::CEvent &Event)
{
	return false;
}

void CEditorObject::OnRender(CUIRect View) {}

void CEditorObject::OnReset() {}
void CEditorObject::OnMapLoad() {}

bool CEditorObject::IsHot()
{
	return UI()->HotItem() == this;
}

void CEditorObject::SetHot()
{
	UI()->SetHotItem(this);
}

void CEditorObject::UnsetHot()
{
	if(IsHot())
		UI()->SetHotItem(nullptr);
}

void CEditorObject::OnHot() {}

bool CEditorObject::IsActive()
{
	return UI()->CheckActiveItem(this);
}

void CEditorObject::SetActive()
{
	SetHot();
	UI()->SetActiveItem(this);
}

void CEditorObject::SetInactive()
{
	if(IsActive())
		UI()->SetActiveItem(nullptr);
}

void CEditorObject::OnActive() {}

CEditor *CEditorObject::Editor() { return m_pEditor; }
const CEditor *CEditorObject::Editor() const { return m_pEditor; }
IInput *CEditorObject::Input() { return m_pEditor->Input(); }
IClient *CEditorObject::Client() { return m_pEditor->Client(); }
CConfig *CEditorObject::Config() { return m_pEditor->Config(); }
IConsole *CEditorObject::Console() { return m_pEditor->Console(); }
IEngine *CEditorObject::Engine() { return m_pEditor->Engine(); }
IGraphics *CEditorObject::Graphics() { return m_pEditor->Graphics(); }
ISound *CEditorObject::Sound() { return m_pEditor->Sound(); }
ITextRender *CEditorObject::TextRender() { return m_pEditor->TextRender(); }
IStorage *CEditorObject::Storage() { return m_pEditor->Storage(); }
CUI *CEditorObject::UI() { return m_pEditor->UI(); }
CRenderTools *CEditorObject::RenderTools() { return m_pEditor->RenderTools(); }
