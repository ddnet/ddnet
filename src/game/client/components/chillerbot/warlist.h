#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H

#include <vector>

#include <game/client/component.h>

class CWarList : public CComponent
{
	struct CWarPlayer
	{
		bool m_HasWar;
		char m_aName[MAX_NAME_LENGTH];
	};

	CWarPlayer m_aWarPlayers[MAX_CLIENTS];
	std::vector<std::string> m_vWarlist;

	static int LoadWarDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	void LoadWarList();
	int LoadWarNames(const char *pFilename);
	bool IsWarlist(const char *pName);

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnInit();

public:
	bool IsWar(int ClientID);
	void SetNameplateColor(int ClientID, STextRenderColor *pColor);
};

#endif
