#include "component.h"

#include "gameclient.h"

#include <base/system.h>

class IKernel *CComponent::Kernel() const { return m_pClient->Kernel(); }
class IEngine *CComponent::Engine() const { return m_pClient->Engine(); }
class IGraphics *CComponent::Graphics() const { return m_pClient->Graphics(); }
class ITextRender *CComponent::TextRender() const { return m_pClient->TextRender(); }
class IInput *CComponent::Input() const { return m_pClient->Input(); }
class IStorage *CComponent::Storage() const { return m_pClient->Storage(); }
class CUi *CComponent::Ui() const { return m_pClient->Ui(); }
class ISound *CComponent::Sound() const { return m_pClient->Sound(); }
class CRenderTools *CComponent::RenderTools() const { return m_pClient->RenderTools(); }
class IConfigManager *CComponent::ConfigManager() const { return m_pClient->ConfigManager(); }
class CConfig *CComponent::Config() const { return m_pClient->Config(); }
class IConsole *CComponent::Console() const { return m_pClient->Console(); }
class IDemoPlayer *CComponent::DemoPlayer() const { return m_pClient->DemoPlayer(); }
class IDemoRecorder *CComponent::DemoRecorder(int Recorder) const { return m_pClient->DemoRecorder(Recorder); }
class IFavorites *CComponent::Favorites() const { return m_pClient->Favorites(); }
class IServerBrowser *CComponent::ServerBrowser() const { return m_pClient->ServerBrowser(); }
class CLayers *CComponent::Layers() const { return m_pClient->Layers(); }
class CCollision *CComponent::Collision() const { return m_pClient->Collision(); }
#if defined(CONF_AUTOUPDATE)
class IUpdater *CComponent::Updater() const
{
	return m_pClient->Updater();
}
#endif

int64_t CComponent::time() const
{
#if defined(CONF_VIDEORECORDER)
	return IVideo::Current() ? IVideo::Time() : time_get();
#else
	return time_get();
#endif
}

float CComponent::LocalTime() const
{
#if defined(CONF_VIDEORECORDER)
	return IVideo::Current() ? IVideo::LocalTime() : Client()->LocalTime();
#else
	return Client()->LocalTime();
#endif
}

class IClient *CComponent::Client() const
{
	return m_pClient->Client();
}

class IHttp *CComponent::Http() const { return m_pClient->Http(); }
