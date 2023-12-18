#ifndef ENGINE_NOTIFICATIONS_H
#define ENGINE_NOTIFICATIONS_H

#include "kernel.h"

class INotifications : public IInterface
{
	MACRO_INTERFACE("notifications")
public:
	virtual void Init(const char *pAppname) = 0;
	virtual void Shutdown() override = 0;
	virtual void Notify(const char *pTitle, const char *pMessage) = 0;
};

INotifications *CreateNotifications();

#endif // ENGINE_NOTIFICATIONS_H
