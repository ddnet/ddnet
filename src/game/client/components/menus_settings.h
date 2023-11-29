#ifndef GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_H
#define GAME_CLIENT_COMPONENTS_MENUS_SETTINGS_H

#include <game/client/components/menus.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

struct IBindListNode
{
	const char *m_pName;

	IBindListNode(const char *pName) :
		m_pName(pName) {}

	virtual bool IsGroup() = 0;
};

struct SBindGroup : IBindListNode
{
	std::vector<std::shared_ptr<IBindListNode>> m_vChildren;
	SFoldableSection m_Section;

	SBindGroup(const char *pName, const std::vector<std::shared_ptr<IBindListNode>> &vChildren) :
		IBindListNode(pName), m_vChildren(vChildren) {}

	bool IsGroup() override { return true; }
};

struct SBindKey : public IBindListNode
{
	const char *m_pCommand;
	std::string m_test;
	int m_KeyId;
	int m_ModifierCombination;

	SBindKey(const char *pName, const char *pCommand) :
		IBindListNode(pName), m_pCommand(pCommand), m_test(pCommand), m_KeyId(0), m_ModifierCombination(0) {}

	bool IsGroup() override { return false; }
};

std::shared_ptr<IBindListNode> BindGroup(const char *pName, const std::vector<std::shared_ptr<IBindListNode>> &vNodes)
{
	return std::make_shared<SBindGroup>(pName, vNodes);
}

std::shared_ptr<IBindListNode> KeyBind(const char *pName, const char *pCommand)
{
	return std::make_shared<SBindKey>(pName, pCommand);
}

#endif