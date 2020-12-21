#ifndef ENGINE_CLIENT_NOTIFICATIONS_H
#define ENGINE_CLIENT_NOTIFICATIONS_H
void NotificationsInit();
void NotificationsUninit();
void NotificationsNotify(const char *pTitle, const char *pMessage);
#endif // ENGINE_CLIENT_NOTIFICATIONS_H
