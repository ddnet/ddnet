/* (c) Rajh, Redix and Sushi. */

#ifndef GAME_CLIENT_COMPONENTS_GHOST_H
#define GAME_CLIENT_COMPONENTS_GHOST_H

#include <game/client/component.h>

enum
{
	GHOSTDATA_TYPE_SKIN = 0,
	GHOSTDATA_TYPE_CHARACTER_NO_TICK,
	GHOSTDATA_TYPE_CHARACTER
};

struct CGhostSkin
{
	int m_Skin0;
	int m_Skin1;
	int m_Skin2;
	int m_Skin3;
	int m_Skin4;
	int m_Skin5;
	int m_UseCustomColor;
	int m_ColorBody;
	int m_ColorFeet;
};

struct CGhostCharacter_NoTick
{
	int m_X;
	int m_Y;
	int m_VelX;
	int m_VelY;
	int m_Angle;
	int m_Direction;
	int m_Weapon;
	int m_HookState;
	int m_HookX;
	int m_HookY;
	int m_AttackTick;
};

struct CGhostCharacter : public CGhostCharacter_NoTick
{
	int m_Tick;
};

class CGhostTools
{
public:
	static void GetGhostCharacter(CGhostCharacter_NoTick *pGhostChar, const CNetObj_Character *pChar)
	{
		pGhostChar->m_X = pChar->m_X;
		pGhostChar->m_Y = pChar->m_Y;
		pGhostChar->m_VelX = pChar->m_VelX;
		pGhostChar->m_VelY = 0;
		pGhostChar->m_Angle = pChar->m_Angle;
		pGhostChar->m_Direction = pChar->m_Direction;
		pGhostChar->m_Weapon = pChar->m_Weapon;
		pGhostChar->m_HookState = pChar->m_HookState;
		pGhostChar->m_HookX = pChar->m_HookX;
		pGhostChar->m_HookY = pChar->m_HookY;
		pGhostChar->m_AttackTick = pChar->m_AttackTick;
	}
};

class CGhost : public CComponent
{
private:
	struct CGhostItem
	{
		int m_ID;
		CTeeRenderInfo m_RenderInfo;
		array<CGhostCharacter_NoTick> m_lPath;

		bool Empty() { return m_lPath.size() == 0; }
		void Reset() { m_lPath.clear(); }

		bool operator==(const CGhostItem &Other) { return m_ID == Other.m_ID; }
	};

	class IGhostLoader *m_pGhostLoader;
	class IGhostRecorder *m_pGhostRecorder;

	array<CGhostItem> m_lGhosts;
	CGhostItem m_CurGhost;

	int m_StartRenderTick;
	int m_CurPos;
	bool m_Recording;
	bool m_Rendering;
	int m_BestTime;

	enum
	{
		RACE_NONE = 0,
		RACE_STARTED,
		RACE_FINISHED,
	};

	void AddInfos(const CNetObj_Character *pChar);

	void StartRecord();
	void StopRecord();
	void StartRender();
	void StopRender();

	void RenderGhost(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev, CTeeRenderInfo *pInfo);
	void RenderGhostHook(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev);

	void Save(int Time);

	void InitRenderInfos(CTeeRenderInfo *pRenderInfo, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

	static void ConGPlay(IConsole::IResult *pResult, void *pUserData);

public:
	CGhost();

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnReset();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnMapLoad();

	void Load(const char* pFilename, int ID);
	void Unload(int ID);

	class IGhostLoader *GhostLoader() const { return m_pGhostLoader; }
	class IGhostRecorder *GhostRecorder() const { return m_pGhostRecorder; }
};

#endif
