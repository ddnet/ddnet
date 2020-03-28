#include <base/detect.h>

#if defined(CONF_LIBNOTIFY)
	#include <libnotify/notify.h>
#endif

#include "notifications.h"

void Notifications::init()
{
	const char *pName = "DDNet Client";
#if defined(CONF_LIBNOTIFY)
	notify_init(pName);
#endif
}

void Notifications::uninit()
{
#if defined(CONF_LIBNOTIFY)
	notify_uninit();
#endif
}

void Notifications::Notify(const char *pTitle, const char *pMsg)
{
#if defined(CONF_LIBNOTIFY)
	NotifyNotification *pNotif = notify_notification_new(pTitle, pMsg, NULL);
	notify_notification_show(pNotif, NULL);
#endif
}
