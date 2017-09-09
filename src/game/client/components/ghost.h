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

class CGhost : public CComponent
{
private:
	enum
	{
		MAX_ACTIVE_GHOSTS = 8,
	};

	struct CGhostItem
	{
		CTeeRenderInfo m_RenderInfo;
		array<CGhostCharacter_NoTick> m_lPath;

		bool Empty() const { return m_lPath.size() == 0; }
		void Reset() { m_lPath.clear(); }
	};

	class IGhostLoader *m_pGhostLoader;
	class IGhostRecorder *m_pGhostRecorder;

	CGhostItem m_aActiveGhosts[MAX_ACTIVE_GHOSTS];
	CGhostItem m_CurGhost;

	int m_StartRenderTick;
	int m_CurPos;
	bool m_Recording;
	bool m_Rendering;

	static void GetGhostCharacter(CGhostCharacter_NoTick *pGhostChar, const CNetObj_Character *pChar);

	void AddInfos(const CNetObj_Character *pChar);
	int GetSlot() const;

	void StartRecord();
	void StopRecord(int Time = -1);
	void StartRender();
	void StopRender();

	void RenderGhost(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev, CTeeRenderInfo *pInfo);
	void RenderGhostHook(const CGhostCharacter_NoTick *pPlayer, const CGhostCharacter_NoTick *pPrev);

	void InitRenderInfos(CTeeRenderInfo *pRenderInfo, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

	static void ConGPlay(IConsole::IResult *pResult, void *pUserData);

public:
	CGhost();

	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual void OnReset();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnMapLoad();

	int Load(const char *pFilename);
	void Unload(int Slot);

	class IGhostLoader *GhostLoader() const { return m_pGhostLoader; }
	class IGhostRecorder *GhostRecorder() const { return m_pGhostRecorder; }
};

#endif
