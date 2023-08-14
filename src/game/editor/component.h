#ifndef GAME_EDITOR_COMPONENT_H
#define GAME_EDITOR_COMPONENT_H

#include <initializer_list>

class CEditor;
class IInput;
class IClient;
class CConfig;
class IConsole;
class IEngine;
class IGraphics;
class ISound;
class ITextRender;
class IStorage;
class CUI;
class CRenderTools;

class CEditorComponent
{
public:
	virtual ~CEditorComponent() = default;

	virtual void Init(CEditor *pEditor);
	virtual void OnRender();

	CEditor *Editor();
	IInput *Input();
	IClient *Client();
	CConfig *Config();
	IConsole *Console();
	IEngine *Engine();
	IGraphics *Graphics();
	ISound *Sound();
	ITextRender *TextRender();
	IStorage *Storage();
	CUI *UI();
	CRenderTools *RenderTools();

private:
	CEditor *m_pEditor;
	IInput *m_pInput;
	IClient *m_pClient;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IEngine *m_pEngine;
	IGraphics *m_pGraphics;
	ISound *m_pSound;
	ITextRender *m_pTextRender;
	IStorage *m_pStorage;
	CUI *m_pUI;
	CRenderTools *m_pRenderTools;
};

#endif
