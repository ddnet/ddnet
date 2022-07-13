/* (c) Redix and Sushi */

#ifndef GAME_CLIENT_COMPONENTS_RACE_DEMO_H
#define GAME_CLIENT_COMPONENTS_RACE_DEMO_H

#include <chrono>
#include <game/client/component.h>

class CRaceDemo : public CComponent
{
	enum
	{
		RACE_NONE = 0,
		RACE_IDLE,
		RACE_PREPARE,
		RACE_STARTED,
		RACE_FINISHED,
	};

	static const char *ms_pRaceDemoDir;

	char m_aTmpFilename[128] = {0};

	int m_RaceState = 0;
	int m_RaceStartTick = 0;
	int m_RecordStopTick = 0;
	int m_Time = 0;

	std::chrono::nanoseconds m_RaceDemosLoadStartTime{0};

	static int RaceDemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);

	void GetPath(char *pBuf, int Size, int Time = -1) const;

	void StopRecord(int Time = -1);
	bool CheckDemo(int Time);

public:
	bool m_AllowRestart = false;

	CRaceDemo();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnReset() override;
	virtual void OnStateChange(int NewState, int OldState) override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnMapLoad() override;
	virtual void OnShutdown() override;

	void OnNewSnapshot();
};
#endif
