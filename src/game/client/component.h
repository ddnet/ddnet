/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENT_H
#define GAME_CLIENT_COMPONENT_H

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif

#include <engine/input.h>

class CGameClient;

/**
* This class is inherited by all the client components.
*
* These components can implement the virtual methods such as OnInit(), OnMessage(int Msg, void *pRawMsg) to provide their functionality.
*/
class CComponent
{
protected:
	friend class CGameClient;

	CGameClient *m_pClient;

	// perhaps propagate pointers for these as well

	/**
	 * Get the kernel interface.
	 */
	class IKernel *Kernel() const;
	class IEngine *Engine() const;
	/**
	 * Get the graphics interface.
	 */
	class IGraphics *Graphics() const;
	/**
	 * Get the text rendering interface.
	 */
	class ITextRender *TextRender() const;
	/**
	 * Get the input interface.
	 */
	class IInput *Input() const;
	/**
	 * Get the storage interface.
	 */
	class IStorage *Storage() const;
	/**
	 * Get the ui interface.
	 */
	class CUi *Ui() const;
	/**
	 * Get the sound interface.
	 */
	class ISound *Sound() const;
	/**
	 * Get the render tools interface.
	 */
	class CRenderTools *RenderTools() const;
	/**
	 * Get the config manager interface.
	 */
	class IConfigManager *ConfigManager() const;
	/**
	 * Get the config interface.
	 */
	class CConfig *Config() const;
	/**
	 * Get the console interface.
	 */
	class IConsole *Console() const;
	/**
	 * Get the demo player interface.
	 */
	class IDemoPlayer *DemoPlayer() const;
	/**
	 * Get the demo recorder interface.
	 *
	 * @param Recorder A member of the RECORDER_x enum
	 * @see RECORDER_MANUAL
	 * @see RECORDER_AUTO
	 * @see RECORDER_RACE
	 * @see RECORDER_REPLAYS
	 */
	class IDemoRecorder *DemoRecorder(int Recorder) const;
	class IFavorites *Favorites() const;
	/**
	 * Get the server browser interface.
	 */
	class IServerBrowser *ServerBrowser() const;
	/**
	 * Get the layers interface.
	 */
	class CLayers *Layers() const;
	/**
	 * Get the collision interface.
	 */
	class CCollision *Collision() const;
#if defined(CONF_AUTOUPDATE)
	/**
	 * Get the updater interface.
	 */
	class IUpdater *Updater() const;
#endif

	/**
	 * Gets the current time.
	 * @see time_get()
	 */
	int64_t time() const;

	/**
	 * Gets the local time.
	 */
	float LocalTime() const;

	/**
	 * Get the http interface
	 */
	class IHttp *Http() const;

public:
	/**
	 * The component virtual destructor.
	 */
	virtual ~CComponent() {}
	/**
	 * Gets the size of the non-abstract component.
	 */
	virtual int Sizeof() const = 0;
	/**
	 * Get a pointer to the game client.
	 */
	class CGameClient *GameClient() const { return m_pClient; }
	/**
	 * Get the client interface.
	 */
	class IClient *Client() const;

	/**
	 * This method is called when the client changes state, e.g from offline to online.
	 * @see IClient::STATE_CONNECTING
	 * @see IClient::STATE_LOADING
	 * @see IClient::STATE_ONLINE
	 */
	virtual void OnStateChange(int NewState, int OldState){};
	/**
	 * Called to let the components register their console commands.
	 */
	virtual void OnConsoleInit(){};
	/**
	 * Called to let the components run initialization code.
	 */
	virtual void OnInit(){};
	/**
	 * Called to cleanup the component.
	 * This method is called when the client is closed.
	 */
	virtual void OnShutdown(){};
	/**
	 * Called to reset the component.
	 * This method is usually called on your component constructor to avoid code duplication.
	 * @see CHud::CHud()
	 * @see CHud::OnReset()
	 */
	virtual void OnReset(){};
	/**
	 * Called when the window has been resized.
	 */
	virtual void OnWindowResize() {}
	/**
	 * Called when skins have been invalidated and must be updated.
	 */
	virtual void OnRefreshSkins() {}
	/**
	 * Called when the component should get rendered.
	 *
	 * The render order depends on the component insertion order.
	 */
	virtual void OnRender(){};
	/**
	 * Called when a new snapshot is received.
	 */
	virtual void OnNewSnapshot(){};
	/**
	 * Called when the input gets released, for example when a text box loses focus.
	 */
	virtual void OnRelease(){};
	/**
	 * Called on map load.
	 */
	virtual void OnMapLoad(){};
	/**
	 * Called when receiving a network message.
	 * @param Msg The message type.
	 * @param pRawMsg The message data.
	 * @see NETMSGTYPE_SV_DDRACETIME
	 * @see CNetMsg_Sv_DDRaceTime
	 */
	virtual void OnMessage(int Msg, void *pRawMsg) {}
	/**
	 * Called on mouse movement, where the x and y values are deltas.
	 *
	 * @param x The amount of change in the x coordinate since the last call.
	 * @param y The amount of change in the y coordinate since the last call.
	 * @param CursorType The type of cursor that caused the movement.
	 */
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) { return false; }
	/**
	 * Called on a input event.
	 * @param Event The input event.
	 */
	virtual bool OnInput(const IInput::CEvent &Event) { return false; }
	/**
	 * Called with all current touch finger states.
	 *
	 * @param vTouchFingerStates The touch finger states to be handled.
	 *
	 * @return `true` if the component used the touch events, `false` otherwise
	 */
	virtual bool OnTouchState(const std::vector<IInput::CTouchFingerState> &vTouchFingerStates) { return false; }
};

#endif
