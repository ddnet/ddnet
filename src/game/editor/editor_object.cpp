#include "editor_object.h"

#include <game/editor/editor.h>

void CEditorObject::OnInit(CEditor *pEditor)
{
	m_pEditor = pEditor;
}

CEditor *CEditorObject::Editor() { return m_pEditor; }
const CEditor *CEditorObject::Editor() const { return m_pEditor; }
IInput *CEditorObject::Input() { return m_pEditor->Input(); }
const IInput *CEditorObject::Input() const { return m_pEditor->Input(); }
IClient *CEditorObject::Client() { return m_pEditor->Client(); }
const IClient *CEditorObject::Client() const { return m_pEditor->Client(); }
CConfig *CEditorObject::Config() { return m_pEditor->Config(); }
const CConfig *CEditorObject::Config() const { return m_pEditor->Config(); }
IEngine *CEditorObject::Engine() { return m_pEditor->Engine(); }
const IEngine *CEditorObject::Engine() const { return m_pEditor->Engine(); }
IGraphics *CEditorObject::Graphics() { return m_pEditor->Graphics(); }
const IGraphics *CEditorObject::Graphics() const { return m_pEditor->Graphics(); }
ISound *CEditorObject::Sound() { return m_pEditor->Sound(); }
const ISound *CEditorObject::Sound() const { return m_pEditor->Sound(); }
ITextRender *CEditorObject::TextRender() { return m_pEditor->TextRender(); }
const ITextRender *CEditorObject::TextRender() const { return m_pEditor->TextRender(); }
IStorage *CEditorObject::Storage() { return m_pEditor->Storage(); }
const IStorage *CEditorObject::Storage() const { return m_pEditor->Storage(); }
CUi *CEditorObject::Ui() { return m_pEditor->Ui(); }
const CUi *CEditorObject::Ui() const { return m_pEditor->Ui(); }
CRenderMap *CEditorObject::RenderMap() { return m_pEditor->RenderMap(); }
const CRenderMap *CEditorObject::RenderMap() const { return m_pEditor->RenderMap(); }
