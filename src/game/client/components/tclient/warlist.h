#ifndef GAME_CLIENT_COMPONENTS_WARLIST_H
#define GAME_CLIENT_COMPONENTS_WARLIST_H

#include <engine/console.h>
#include <engine/shared/protocol.h>
#include <game/client/component.h>

#define WARLIST_FILE "tclient_warlist.cfg"

enum
{
	MAX_WARLIST_TYPE_LENGTH = 16,
	MAX_WARLIST_IMPORT_ID_LENGTH = 16,
	MAX_WARLIST_REASON_LENGTH = 256
};

// TODO, add chat prefix
class CWarType
{
public:
	char m_aWarType[MAX_WARLIST_TYPE_LENGTH] = "";
	ColorRGBA m_Color = ColorRGBA(1, 1, 1, 1);
	bool m_Removable = true;
	bool m_Imported = false;

	CWarType(const char *pName, ColorRGBA Color = ColorRGBA(1, 1, 1, 1), bool Removable = true, bool IsImport = false)
	{
		str_copy(m_aWarType, pName);
		m_Color = Color;
		m_Removable = Removable;
		m_Imported = IsImport;
	}

	bool operator==(const CWarType &Other) const
	{
		return str_comp(m_aWarType, Other.m_aWarType) == 0;
	}
};

class CWarEntry
{
public:
	// a war entry can have a name, or clan, but not both
	// name matches the user with that name
	// clan matches all users with that clan
	char m_aName[MAX_NAME_LENGTH] = "";
	char m_aClan[MAX_CLAN_LENGTH] = "";
	char m_aReason[MAX_WARLIST_REASON_LENGTH] = "";

	CWarType *m_pWarType = nullptr;
	bool m_Imported = false;

	CWarEntry(CWarType *pWarType)
	{
		m_pWarType = pWarType;
	}

	CWarEntry(CWarType *pWarType, const char *pName, const char *pClan, const char *pReason, bool IsImport = false)
	{
		m_pWarType = pWarType;
		str_copy(m_aName, pName);
		str_copy(m_aClan, pClan);
		str_copy(m_aReason, pReason);
		m_Imported = IsImport;
	}

	bool operator==(const CWarEntry &Other) const
	{
		return (str_comp(m_aName, Other.m_aName) == 0 || str_comp(m_aClan, Other.m_aClan) == 0) && m_pWarType == Other.m_pWarType;
	}
};

class CWarDataCache
{
public:
	ColorRGBA m_NameColor = ColorRGBA(1, 1, 1, 1);
	ColorRGBA m_ClanColor = ColorRGBA(1, 1, 1, 1);
	bool IsWarName = false;
	bool IsWarClan = false;
	char m_aReason[MAX_WARLIST_REASON_LENGTH] = "";
};

class CWarList : public CComponent
{
	// The war list will have these commands
	//
	// Preset Commands
	// .war [name] [reason]
	// .clanwar [clan] [reason]
	// .team [name] [reason]
	// .clanteam [name] [reason]
	// .delwar [name]
	// .delteam [name]
	// .delclanwar [clan]
	// .delclanteam [team]
	//
	// Generic Commands
	// .name [type] [name] [reason]
	// .clan [type] [clan] [reason]
	// .delname [type] [name]
	// .delclan [type] [clan]
	//
	// Backend commands
	// update_war_type [index] [type] [color]
	// add_war_entry [type] [name] [clan] [reason]

	// Preset war Commands
	static void ConNameWar(IConsole::IResult *pResult, void *pUserData);
	static void ConClanWar(IConsole::IResult *pResult, void *pUserData);
	static void ConNameTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConClanTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveNameWar(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveNameTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanWar(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanTeam(IConsole::IResult *pResult, void *pUserData);

	// Generic Commands
	static void ConName(IConsole::IResult *pResult, void *pUserData);
	static void ConClan(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveName(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClan(IConsole::IResult *pResult, void *pUserData);

	// Backend Commands for config file
	static void ConAddWarEntry(IConsole::IResult *pResult, void *pUserData);
	static void ConUpsertWarType(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void WriteLine(const char *pLine);
	class IStorage *m_pStorage;
	IOHANDLE m_WarlistFile;

public:
	// None type war entries will float to the top of the list, so they can be assigned a type
	CWarType m_WarTypeNone = CWarType("none", ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false);

	// duplicate war types are NOT allowed
	std::vector<CWarType> m_WarTypes = {CWarType("war", ColorRGBA(1.0f, 0.2f, 0.2f, 1.0f), false), CWarType("team", ColorRGBA(0.0f, 0.9f, 0.2f, 1.0f), false)};

	// TODO: add a special backend command that allows for changing the names of the default war types

	// Duplicate war entries ARE allowed
	std::vector<CWarEntry> m_WarEntries;
	// TODO: create an unordered map for war clans and war names, to speed up updating the WarPlayers cache
	// It should be updated when m_WarList changes

	CWarDataCache m_WarPlayers[MAX_CLIENTS];

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnNewSnapshot() override;
	virtual void OnConsoleInit() override;

	void UpdateWarPlayers();

	void UpdateWarEntry(int Index, const char *pName, const char *pClan, const char *pReason, CWarType *pType);

	// Adds a new war entry if the specified index is not found
	void UpsertWarType(int Index, const char *pType, ColorRGBA Color);

	void AddWarEntry(const char *pName, const char *pClan, const char *pReason, const char *pType);
	void AddWarType(const char *pType, ColorRGBA Color);

	void RemoveWarEntry(const char *pName, const char *pClan, const char *pType);
	void RemoveWarType(const char *pType);

	void RemoveWarEntry(int Index);
	void RemoveWarType(int Index);

	ColorRGBA GetNameplateColor(int ClientId);
	ColorRGBA GetClanColor(int ClientId);

	void GetReason(char *pReason, int ClientId);
	CWarDataCache GetWarData(int ClientId);

	void SortWarEntries();

	CWarType *FindWarType(const char *pType);
	CWarEntry *FindWarEntry(const char *pName, const char *pClan, const char *pType);
};

#endif
