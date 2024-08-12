#ifndef GAME_EDITOR_EDITOR_OBJECT_H
#define GAME_EDITOR_EDITOR_OBJECT_H

#include <functional>

#include <engine/input.h>
#include <game/client/ui_rect.h>

class CUi;
class CEditor;
class IClient;
class CConfig;
class IConsole;
class IEngine;
class IGraphics;
class ISound;
class ITextRender;
class IStorage;
class CRenderTools;

class CEditorObject
{
public:
	virtual ~CEditorObject() = default;

	/**
	 * Initialise the component and interface pointers.
	 * Needs to be the first function that is called.
	 * The default implentation also resets the component.
	 */
	virtual void OnInit(CEditor *pEditor);

	/**
	 * Maybe calls `OnHot` or `OnActive`.
	 */
	virtual void OnUpdate();

	/**
	 * Gets called before `OnRender`. Should return true
	 * if the event was consumed.
	 */
	virtual bool OnInput(const IInput::CEvent &Event);

	virtual void OnRender(CUIRect View);

	/**
	 * Gets called after `OnRender` when the component is hot but not active.
	 */
	virtual void OnHot();

	/**
	 * Gets called after `OnRender` when the component is active.
	 */
	virtual void OnActive();

	virtual void OnReset();
	virtual void OnMapLoad();

	bool IsHot();
	void SetHot();
	void UnsetHot();

	bool IsActive();
	void SetActive();
	void SetInactive();

	CEditor *Editor();
	const CEditor *Editor() const;
	IInput *Input();
	IClient *Client();
	CConfig *Config();
	IConsole *Console();
	IEngine *Engine();
	IGraphics *Graphics();
	ISound *Sound();
	ITextRender *TextRender();
	IStorage *Storage();
	CUi *Ui();
	CRenderTools *RenderTools();

private:
	CEditor *m_pEditor;
};

#endif
