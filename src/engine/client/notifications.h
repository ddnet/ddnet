#ifndef ENGINE_CLIENT_NOTIFICATIONS_H
#define ENGINE_CLIENT_NOTIFICATIONS_H

#include <engine/notifications.h>

class CNotifications : public INotifications
{
public:
	void Init(const char *pAppName) override;
	void Shutdown() override;
	void Notify(const char *pTitle, const char *pMessage) override;

private:
	bool m_Initialized = false;
	char m_aAppName[64];
};

#endif // ENGINE_CLIENT_NOTIFICATIONS_H
