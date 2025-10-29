#ifndef GAME_EDITOR_COMPONENT_H
#define GAME_EDITOR_COMPONENT_H

#include <engine/input.h>

#include <game/client/ui_rect.h>
#include <game/editor/editor_object.h>

#include <functional>
#include <vector>

class CEditorComponent : public CEditorObject
{
public:
	/**
	 * Initialize the component and interface pointers.
	 * Needs to be the first function that is called.
	 * The default implementation also resets the component.
	 */
	void OnInit(CEditor *pEditor) override;

	virtual void OnReset();

	virtual void OnMapLoad();

	/**
	 * Gets called before @link OnRender @endlink. Should return `true`
	 * if the event was consumed.
	 */
	virtual bool OnInput(const IInput::CEvent &Event);

	virtual void OnUpdate();

	virtual void OnRender(CUIRect View);

	/**
	 * Initialize all registered subcomponents.
	 * Needs to be called after the interfaces have been initialized.
	 */
	void InitSubComponents();

	// Register a new subcomponent.
	void RegisterSubComponent(CEditorComponent &Component);

private:
	std::vector<std::reference_wrapper<CEditorComponent>> m_vSubComponents;
};

#endif
