/* (c) Redix and Sushi */

#ifndef GAME_CLIENT_COMPONENTS_RACE_DEMO_H
#define GAME_CLIENT_COMPONENTS_RACE_DEMO_H

#include <game/client/gameclient.h>

#include <game/client/component.h>

class CRaceDemo : public CComponent
{
	int m_RecordStopTime;
	int m_DemoStartTick;
	int m_Time;
	const char *m_pMap;

	void Stop();

public:
	bool m_IsSolo;
	int m_RaceState;

	enum
	{
		RACE_NONE = 0,
		RACE_STARTED,
		RACE_FINISHED,
	};

	CRaceDemo();

	virtual void OnReset();
	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnRender();
	virtual void OnShutdown();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnMapLoad();

	void CheckDemo();
	void SaveDemo(const char* pDemo);
};
#endif
