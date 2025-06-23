#include "component.h"

#include "gameclient.h"

#include <base/system.h>

void CComponentInterfaces::OnInterfacesInit(CGameClient *pClient)
{
	dbg_assert(m_pClient == nullptr, "Component already initialized");
	m_pClient = pClient;
}

class IKernel *CComponentInterfaces::Kernel() const
{
	return m_pClient->Kernel();
}

class IEngine *CComponentInterfaces::Engine() const
{
	return m_pClient->Engine();
}

class IGraphics *CComponentInterfaces::Graphics() const
{
	return m_pClient->Graphics();
}

class ITextRender *CComponentInterfaces::TextRender() const
{
	return m_pClient->TextRender();
}

class IInput *CComponentInterfaces::Input() const
{
	return m_pClient->Input();
}

class IStorage *CComponentInterfaces::Storage() const
{
	return m_pClient->Storage();
}

class CUi *CComponentInterfaces::Ui() const
{
	return m_pClient->Ui();
}

class ISound *CComponentInterfaces::Sound() const
{
	return m_pClient->Sound();
}

class CRenderTools *CComponentInterfaces::RenderTools() const
{
	return m_pClient->RenderTools();
}

class IConfigManager *CComponentInterfaces::ConfigManager() const
{
	return m_pClient->ConfigManager();
}

class CConfig *CComponentInterfaces::Config() const
{
	return m_pClient->Config();
}

class IConsole *CComponentInterfaces::Console() const
{
	return m_pClient->Console();
}

class IDemoPlayer *CComponentInterfaces::DemoPlayer() const
{
	return m_pClient->DemoPlayer();
}

class IDemoRecorder *CComponentInterfaces::DemoRecorder(int Recorder) const
{
	return m_pClient->DemoRecorder(Recorder);
}

class IFavorites *CComponentInterfaces::Favorites() const
{
	return m_pClient->Favorites();
}

class IServerBrowser *CComponentInterfaces::ServerBrowser() const
{
	return m_pClient->ServerBrowser();
}

class CLayers *CComponentInterfaces::Layers() const
{
	return m_pClient->Layers();
}

class CCollision *CComponentInterfaces::Collision() const
{
	return m_pClient->Collision();
}

#if defined(CONF_AUTOUPDATE)
class IUpdater *CComponentInterfaces::Updater() const
{
	return m_pClient->Updater();
}
#endif

int64_t CComponentInterfaces::time() const
{
#if defined(CONF_VIDEORECORDER)
	return IVideo::Current() ? IVideo::Time() : time_get();
#else
	return time_get();
#endif
}

float CComponentInterfaces::LocalTime() const
{
#if defined(CONF_VIDEORECORDER)
	return IVideo::Current() ? IVideo::LocalTime() : Client()->LocalTime();
#else
	return Client()->LocalTime();
#endif
}

class IClient *CComponentInterfaces::Client() const
{
	return m_pClient->Client();
}

class IHttp *CComponentInterfaces::Http() const
{
	return m_pClient->Http();
}
