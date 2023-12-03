#include "notifications.h"

#include <base/detect.h>

#if defined(CONF_PLATFORM_MACOS)
// Code is in src/macos/notification.mm.
void NotificationsNotifyMacOsInternal(const char *pTitle, const char *pMessage);
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_HAIKU) && !defined(CONF_WEBASM)
#include <libnotify/notify.h>
#define NOTIFICATIONS_USE_LIBNOTIFY
#endif

void CNotifications::Init(const char *pAppname)
{
#if defined(NOTIFICATIONS_USE_LIBNOTIFY)
	notify_init(pAppname);
#endif
}

void CNotifications::Shutdown()
{
#if defined(NOTIFICATIONS_USE_LIBNOTIFY)
	notify_uninit();
#endif
}

void CNotifications::Notify(const char *pTitle, const char *pMessage)
{
#if defined(CONF_PLATFORM_MACOS)
	NotificationsNotifyMacOsInternal(pTitle, pMessage);
#elif defined(NOTIFICATIONS_USE_LIBNOTIFY)
	NotifyNotification *pNotif = notify_notification_new(pTitle, pMessage, "ddnet");
	if(pNotif)
	{
		notify_notification_show(pNotif, NULL);
		g_object_unref(G_OBJECT(pNotif));
	}
#endif
}

INotifications *CreateNotifications()
{
	return new CNotifications();
}
