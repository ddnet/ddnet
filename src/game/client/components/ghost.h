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
		MAX_ACTIVE_GHOSTS = 256,
	};

	class CGhostPath
	{
		int m_ChunkSize;
		int m_NumItems;

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

		void Add(const CGhostCharacter &Char);
		CGhostCharacter *Get(int Index);
	};

	class CGhostItem
	{
	public:
		CTeeRenderInfo m_RenderInfo;
		CGhostSkin m_Skin;
		CGhostPath m_Path;
		int m_StartTick;
		char m_aPlayer[MAX_NAME_LENGTH];
		int m_PlaybackPos;

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

	class IGhostLoader *m_pGhostLoader;
	class IGhostRecorder *m_pGhostRecorder;

	CGhostItem m_aActiveGhosts[MAX_ACTIVE_GHOSTS];
	CGhostItem m_CurGhost;

	char m_aTmpFilename[IO_MAX_PATH_LENGTH];

	int m_NewRenderTick = -1;
	int m_StartRenderTick = -1;
	int m_LastDeathTick = -1;
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
	bool m_AllowRestart;

	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	virtual void OnReset() override;
	virtual void OnRefreshSkins() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnMapLoad() override;
	virtual void OnShutdown() override;
	virtual void OnNewSnapshot() override;

	void OnNewPredictedSnapshot();

	int FreeSlots() const;
	int Load(const char *pFilename);
	void Unload(int Slot);
	void UnloadAll();

	void SaveGhost(CMenus::CGhostItem *pItem);

	const char *GetGhostDir() const { return ms_pGhostDir; }

	class IGhostLoader *GhostLoader() const { return m_pGhostLoader; }
	class IGhostRecorder *GhostRecorder() const { return m_pGhostRecorder; }
};

#endif
