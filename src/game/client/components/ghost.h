/* (c) Rajh, Redix and Sushi. */

#ifndef GAME_CLIENT_COMPONENTS_GHOST_H
#define GAME_CLIENT_COMPONENTS_GHOST_H

#include <game/client/component.h>
#include <game/client/components/menus.h>
#include <game/generated/protocol.h>

#include <game/client/render.h>

struct CNetObj_Character;

enum
{
	GHOSTDATA_TYPE_SKIN = 0,
	GHOSTDATA_TYPE_CHARACTER_NO_TICK,
	GHOSTDATA_TYPE_CHARACTER,
	GHOSTDATA_TYPE_START_TICK
};

struct CGhostSkin
{
	int m_Skin0 = 0;
	int m_Skin1 = 0;
	int m_Skin2 = 0;
	int m_Skin3 = 0;
	int m_Skin4 = 0;
	int m_Skin5 = 0;
	int m_UseCustomColor = 0;
	int m_ColorBody = 0;
	int m_ColorFeet = 0;
};

struct CGhostCharacter_NoTick
{
	int m_X = 0;
	int m_Y = 0;
	int m_VelX = 0;
	int m_VelY = 0;
	int m_Angle = 0;
	int m_Direction = 0;
	int m_Weapon = 0;
	int m_HookState = 0;
	int m_HookX = 0;
	int m_HookY = 0;
	int m_AttackTick = 0;
};

struct CGhostCharacter : public CGhostCharacter_NoTick
{
	int m_Tick = 0;
};

class CGhost : public CComponent
{
private:
	enum
	{
		MAX_ACTIVE_GHOSTS = 8,
	};

	class CGhostPath
	{
		int m_ChunkSize = 0;
		int m_NumItems = 0;

		std::vector<CGhostCharacter *> m_vpChunks;

	public:
		CGhostPath() { Reset(); }
		~CGhostPath() { Reset(); }
		CGhostPath(const CGhostPath &Other) = delete;
		CGhostPath &operator=(const CGhostPath &Other) = delete;

		CGhostPath(CGhostPath &&Other) noexcept;
		CGhostPath &operator=(CGhostPath &&Other) noexcept;

		void Reset(int ChunkSize = 25 * 60); // one minute with default snap rate
		void SetSize(int Items);
		int Size() const { return m_NumItems; }

		void Add(CGhostCharacter Char);
		CGhostCharacter *Get(int Index);
	};

	class CGhostItem
	{
	public:
		CTeeRenderInfo m_RenderInfo;
		CGhostSkin m_Skin;
		CGhostPath m_Path;
		int m_StartTick = 0;
		char m_aPlayer[MAX_NAME_LENGTH] = {0};
		int m_PlaybackPos = 0;

		CGhostItem() { Reset(); }

		bool Empty() const { return m_Path.Size() == 0; }
		void Reset()
		{
			m_Path.Reset();
			m_StartTick = -1;
			m_PlaybackPos = -1;
		}
	};

	static const char *ms_pGhostDir;

	class IGhostLoader *m_pGhostLoader = nullptr;
	class IGhostRecorder *m_pGhostRecorder = nullptr;

	CGhostItem m_aActiveGhosts[MAX_ACTIVE_GHOSTS];
	CGhostItem m_CurGhost;

	char m_aTmpFilename[128] = {0};

	int m_NewRenderTick = 0;
	int m_StartRenderTick = 0;
	int m_LastDeathTick = 0;
	int m_LastRaceTick = 0;
	bool m_Recording = false;
	bool m_Rendering = false;

	bool m_RenderingStartedByServer = false;

	static void GetGhostSkin(CGhostSkin *pSkin, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);
	static void GetGhostCharacter(CGhostCharacter *pGhostChar, const CNetObj_Character *pChar, const CNetObj_DDNetCharacter *pDDnetChar);
	static void GetNetObjCharacter(CNetObj_Character *pChar, const CGhostCharacter *pGhostChar);

	void GetPath(char *pBuf, int Size, const char *pPlayerName, int Time = -1) const;

	void AddInfos(const CNetObj_Character *pChar, const CNetObj_DDNetCharacter *pDDnetChar);
	int GetSlot() const;

	void CheckStart();
	void CheckStartLocal(bool Predicted);
	void TryRenderStart(int Tick, bool ServerControl);

	void StartRecord(int Tick);
	void StopRecord(int Time = -1);
	void StartRender(int Tick);
	void StopRender();

	void InitRenderInfos(CGhostItem *pGhost);

	static void ConGPlay(IConsole::IResult *pResult, void *pUserData);

public:
	bool m_AllowRestart = false;

	CGhost();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	virtual void OnReset() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnMapLoad() override;
	virtual void OnShutdown() override;

	void OnNewSnapshot();
	void OnNewPredictedSnapshot();

	int FreeSlots() const;
	int Load(const char *pFilename);
	void Unload(int Slot);
	void UnloadAll();

	void SaveGhost(CMenus::CGhostItem *pItem);

	const char *GetGhostDir() const { return ms_pGhostDir; }

	class IGhostLoader *GhostLoader() const { return m_pGhostLoader; }
	class IGhostRecorder *GhostRecorder() const { return m_pGhostRecorder; }

	int GetLastRaceTick();

	void RefindSkin();
};

#endif
