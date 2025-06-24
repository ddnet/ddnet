#include "notifications.h"

#include <base/system.h>

#if defined(CONF_PLATFORM_MACOS)
// Code is in src/macos/notification.mm.
void NotificationsNotifyMacOsInternal(const char *pTitle, const char *pMessage);
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_HAIKU) && !defined(CONF_PLATFORM_EMSCRIPTEN)
#include <libnotify/notify.h>
#define NOTIFICATIONS_USE_LIBNOTIFY
#elif defined(CONF_FAMILY_WINDOWS)
#include <Windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::Data::Xml::Dom;
#endif

void CNotifications::Init(const char *pAppName)
{
	str_copy(m_aAppName, pAppName, sizeof(m_aAppName));
#if defined(NOTIFICATIONS_USE_LIBNOTIFY)
	notify_init(pAppName);
#elif defined(CONF_FAMILY_WINDOWS)

	// check if toast api is available
	HMODULE LibCombase = LoadLibraryW(L"combase.dll");
	if(!LibCombase)
		return;
	FARPROC RoInitializeProc = GetProcAddress(LibCombase, "RoInitialize");
	FreeLibrary(LibCombase);
	if(RoInitializeProc == nullptr)
		return;

	// assign an AUMID
	HINSTANCE LibShell32 = LoadLibraryW(L"SHELL32.DLL");
	if(!LibShell32)
		return;

	auto SetAppUserModelProc = GetProcAddress(LibShell32, "SetCurrentProcessExplicitAppUserModelID");
	if(SetAppUserModelProc == nullptr)
	{
		FreeLibrary(LibShell32);
		return;
	}

	auto SetCurrentProcessExplicitAppUserModelID = reinterpret_cast<HRESULT(FAR STDAPICALLTYPE *)(__in PCWSTR)>(SetAppUserModelProc);
	SetCurrentProcessExplicitAppUserModelID(windows_utf8_to_wide(m_aAppName).c_str());
	FreeLibrary(LibShell32);
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
	char aXml[1024];
	str_format(aXml, sizeof(aXml), R"(
        <toast>
            <visual version='1'>
                <binding template='ToastText01'>
                    <text id='1'>%s</text>
                    <text placement='attribution'>%s</text>
                </binding>
            </visual>
        </toast>
    )",
		pMessage, pTitle);

	XmlDocument ToastXml;
	ToastXml.LoadXml(windows_utf8_to_wide(aXml));
	ToastNotification Toast(ToastXml);

	Toast.ExpirationTime(winrt::clock::now() + std::chrono::seconds(5));

	ToastNotifier Notifier = ToastNotificationManager::CreateToastNotifier(windows_utf8_to_wide(m_aAppName));
	Notifier.Show(Toast);
#endif
}

INotifications *CreateNotifications()
{
	return new CNotifications();
}
