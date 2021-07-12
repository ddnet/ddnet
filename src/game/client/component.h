/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENT_H
#define GAME_CLIENT_COMPONENT_H

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif

#include <base/color.h>
#include <engine/input.h>

#include <engine/client.h>
#include <engine/console.h>
#include <game/localization.h>

#include <engine/config.h>

class CGameClient;

class CComponent
{
protected:
	friend class CGameClient;

	CGameClient *m_pClient;

	// perhaps propagte pointers for these as well
	class IKernel *Kernel() const;
	class IGraphics *Graphics() const;
	class ITextRender *TextRender() const;
	class IInput *Input() const;
	class IStorage *Storage() const;
	class CUI *UI() const;
	class ISound *Sound() const;
	class CRenderTools *RenderTools() const;
	class CConfig *Config() const;
	class IConsole *Console() const;
	class IDemoPlayer *DemoPlayer() const;
	class IDemoRecorder *DemoRecorder(int Recorder) const;
	class IServerBrowser *ServerBrowser() const;
	class CLayers *Layers() const;
	class CCollision *Collision() const;
#if defined(CONF_AUTOUPDATE)
	class IUpdater *Updater() const;
#endif

#if defined(CONF_VIDEORECORDER)
	int64_t time() const
	{
		return IVideo::Current() ? IVideo::Time() : time_get();
	}
#else
	int64_t time() const
	{
		return time_get();
	}
#endif
	float LocalTime() const;

public:
	virtual ~CComponent() {}
	class CGameClient *GameClient() const { return m_pClient; }
	class IClient *Client() const;

	virtual void OnStateChange(int NewState, int OldState){};
	virtual void OnConsoleInit(){};
	virtual void OnInit(){};
	virtual void OnReset(){};
	virtual void OnWindowResize() {}
	virtual void OnRender(){};
	virtual void OnRelease(){};
	virtual void OnMapLoad(){};
	virtual void OnMessage(int Msg, void *pRawMsg) {}
	virtual bool OnMouseMove(float x, float y) { return false; }
	virtual bool OnInput(IInput::CEvent e) { return false; }
};

#endif
