#ifndef GAME_EDITOR_EDITOR_OBJECT_H
#define GAME_EDITOR_EDITOR_OBJECT_H

class CEditor;
class IInput;
class IClient;
class CConfig;
class IEngine;
class IGraphics;
class ISound;
class ITextRender;
class IStorage;
class CUi;
class CRenderTools;
class CRenderMap;

class CEditorObject
{
public:
	virtual ~CEditorObject() = default;

	/**
	 * Initialize the interface pointers.
	 * Needs to be the first function that is called.
	 */
	virtual void OnInit(CEditor *pEditor);

	CEditor *Editor();
	const CEditor *Editor() const;
	IInput *Input();
	const IInput *Input() const;
	IClient *Client();
	const IClient *Client() const;
	CConfig *Config();
	const CConfig *Config() const;
	IEngine *Engine();
	const IEngine *Engine() const;
	IGraphics *Graphics();
	const IGraphics *Graphics() const;
	ISound *Sound();
	const ISound *Sound() const;
	ITextRender *TextRender();
	const ITextRender *TextRender() const;
	IStorage *Storage();
	const IStorage *Storage() const;
	CUi *Ui();
	const CUi *Ui() const;
	CRenderMap *RenderMap();
	const CRenderMap *RenderMap() const;

private:
	CEditor *m_pEditor;
};

#endif
