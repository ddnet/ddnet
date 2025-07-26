#include "editor_object.h"

#include <game/editor/editor.h>

void CEditorObject::OnInit(CEditor *pEditor)
{
	m_pEditor = pEditor;
}

CEditor *CEditorObject::Editor() const
{
	return m_pEditor;
}

IInput *CEditorObject::Input() const
{
	return m_pEditor->Input();
}

IClient *CEditorObject::Client() const
{
	return m_pEditor->Client();
}

CConfig *CEditorObject::Config() const
{
	return m_pEditor->Config();
}

IConsole *CEditorObject::Console() const
{
	return m_pEditor->Console();
}

IEngine *CEditorObject::Engine() const
{
	return m_pEditor->Engine();
}

IGraphics *CEditorObject::Graphics() const
{
	return m_pEditor->Graphics();
}

ISound *CEditorObject::Sound() const
{
	return m_pEditor->Sound();
}

ITextRender *CEditorObject::TextRender() const
{
	return m_pEditor->TextRender();
}

IStorage *CEditorObject::Storage() const
{
	return m_pEditor->Storage();
}

CUi *CEditorObject::Ui() const
{
	return m_pEditor->Ui();
}

CRenderTools *CEditorObject::RenderTools() const
{
	return m_pEditor->RenderTools();
}
