#include "notifications.h"

#include <base/detect.h>

#if defined(CONF_PLATFORM_MACOS)
// Code is in src/macos/notification.mm.
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_HAIKU) && !defined(CONF_WEBASM)
#include <libnotify/notify.h>
void NotificationsInit()
{
	notify_init("DDNet Client");
}
void NotificationsUninit()
{
	notify_uninit();
}
void NotificationsNotify(const char *pTitle, const char *pMessage)
{
	NotifyNotification *pNotif = notify_notification_new(pTitle, pMessage, "ddnet");
	notify_notification_show(pNotif, NULL);
	g_object_unref(G_OBJECT(pNotif));
}
#else
void NotificationsInit()
{
}
void NotificationsUninit()
{
}
void NotificationsNotify(const char *pTitle, const char *pMessage)
{
	(void)pTitle;
	(void)pMessage;
}
#endif
