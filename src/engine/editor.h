/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_EDITOR_H
#define ENGINE_EDITOR_H
#include "kernel.h"

class IEditor : public IInterface
{
	MACRO_INTERFACE("editor")
public:
	virtual ~IEditor() {}
	virtual void Init() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnActivate() = 0;
	virtual void OnWindowResize() = 0;
	virtual void OnClose() = 0;
	virtual void OnDialogClose() = 0;
	virtual bool HasUnsavedData() const = 0;
	virtual bool HandleMapDrop(const char *pFilename, int StorageType) = 0;
	virtual bool Load(const char *pFilename, int StorageType) = 0;
	virtual bool Save(const char *pFilename) = 0;
	virtual void UpdateMentions() = 0;
	virtual void ResetMentions() = 0;
	virtual void OnIngameMoved() = 0;
	virtual void ResetIngameMoved() = 0;
};

extern IEditor *CreateEditor();
#endif
