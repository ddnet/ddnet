#include "notifications.h"

#include <base/detect.h>

#if defined(CONF_PLATFORM_MACOS)
// Code is in src/macos/notification.mm.
void NotificationsNotifyMacOsInternal(const char *pTitle, const char *pMessage);
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_HAIKU) && !defined(CONF_PLATFORM_EMSCRIPTEN)
#include <libnotify/notify.h>
#define NOTIFICATIONS_USE_LIBNOTIFY
#elif defined(CONF_FAMILY_WINDOWS)
#include <base/system.h>
#include <engine/external/wintoast/wintoastlib.h>
using namespace WinToastLib;

class CCustomToastHandler : public IWinToastHandler
{
public:
	void toastActivated() const override {}
	void toastActivated(int ActionIndex) const override {}
	void toastActivated(std::wstring Response) const override {}
	void toastDismissed(WinToastDismissalReason State) const override {}
	void toastFailed() const override {}
};

#endif

void CNotifications::Init(const char *pAppName)
{
#if defined(NOTIFICATIONS_USE_LIBNOTIFY)
	notify_init(pAppName);
#elif defined(CONF_FAMILY_WINDOWS)
	if(!WinToast::isCompatible())
	{
		return;
	}

	WinToast::instance()->setAppName(windows_utf8_to_wide(pAppName));
	WinToast::instance()->setAppUserModelId(windows_utf8_to_wide(pAppName));

	if(!WinToast::instance()->initialize())
	{
		return;
	}
#endif

	m_Initialized = true;
}

void CNotifications::Shutdown()
{
#if defined(NOTIFICATIONS_USE_LIBNOTIFY)
	notify_uninit();
#endif
}

void CNotifications::Notify(const char *pTitle, const char *pMessage)
{
	if(!m_Initialized)
		return;
#if defined(CONF_PLATFORM_MACOS)
	NotificationsNotifyMacOsInternal(pTitle, pMessage);
#elif defined(NOTIFICATIONS_USE_LIBNOTIFY)
	NotifyNotification *pNotif = notify_notification_new(pTitle, pMessage, "ddnet");
	if(pNotif)
	{
		notify_notification_show(pNotif, NULL);
		g_object_unref(G_OBJECT(pNotif));
	}
#elif defined(CONF_FAMILY_WINDOWS)
	WinToastTemplate Toast(WinToastTemplate::Text01);
	Toast.setTextField(windows_utf8_to_wide(pMessage), WinToastTemplate::FirstLine);
	Toast.setAttributionText(windows_utf8_to_wide(pTitle));
	Toast.setDuration(WinToastTemplate::Duration::System);
	Toast.setExpiration(5000);

	WinToast::instance()->showToast(Toast, new CCustomToastHandler());
#endif
}

INotifications *CreateNotifications()
{
	return new CNotifications();
}
