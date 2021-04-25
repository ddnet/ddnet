#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_WARLIST_H

#include <vector>

#include <game/client/component.h>

class CWarList : public CComponent
{
	struct CWarPlayer
	{
		bool m_IsWar;
		bool m_IsTeam;
		char m_aName[MAX_NAME_LENGTH];
	};

	CWarPlayer m_aWarPlayers[MAX_CLIENTS];
	std::vector<std::string> m_vWarlist;
	std::vector<std::string> m_vTeamlist;

	static int LoadWarDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	static int LoadTeamDir(const char *pImgName, int IsDir, int DirType, void *pUser);
	void LoadWarList();
	void LoadTeamList();
	int LoadWarNames(const char *pFilename);
	int LoadTeamNames(const char *pFilename);
	bool IsWarlist(const char *pName);
	bool IsTeamlist(const char *pName);


	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConchainWarList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	bool IsWar(int ClientID);
	bool IsTeam(int ClientID);
	void SetNameplateColor(int ClientID, STextRenderColor *pColor);
};

#endif
