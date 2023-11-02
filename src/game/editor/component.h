#ifndef GAME_EDITOR_COMPONENT_H
#define GAME_EDITOR_COMPONENT_H

#include "editor_object.h"

#include <vector>

class CEditorComponent : public CEditorObject
{
public:
	/**
	 * Gets called before `OnRender`. Should return true
	 * if the event was consumed. By default the events
	 * are forwarded to the subcomponents.
	 */
	virtual bool OnInput(const IInput::CEvent &Event) override;

	/**
	 * Initialise all registered subcomponents.
	 * Needs to be called after the interfaces have been initialised.
	 */
	void InitSubComponents();

	// Register a new subcomponent.
	void RegisterSubComponent(CEditorComponent &Component);

private:
	std::vector<std::reference_wrapper<CEditorComponent>> m_vSubComponents = {};
};

#endif
