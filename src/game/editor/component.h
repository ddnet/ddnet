#ifndef GAME_EDITOR_COMPONENT_H
#define GAME_EDITOR_COMPONENT_H

#include <functional>
#include <game/client/ui.h>

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
class CRenderTools;

class CEditorComponent
{
public:
	virtual ~CEditorComponent() = default;

	/**
	 * Initialise the component and interface pointers.
	 * Needs to be the first function that is called.
	 */
	virtual void Init(CEditor *pEditor);

	/**
	 * Calls `OnRender` and then maybe `OnHot` or `OnActive`.
	 */
	void OnUpdate(CUIRect View);

	/**
	 * Gets called before `OnRender`. Should return true
	 * if the event was consumed.
	 */
	virtual bool OnInput(const IInput::CEvent &Event);

	virtual void OnRender(CUIRect View);

	/**
	 * Gets called after `OnRender` when the component is hot but not active.
	 * I
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

	/**
	 * Initialise all registered subcomponents.
	 * Needs to be called after the interfaces have been initialised.
	 */
	void InitSubComponents();
	void RegisterSubComponent(CEditorComponent &Component);

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

	std::vector<std::reference_wrapper<CEditorComponent>> m_vSubComponents = {};
};

#endif
