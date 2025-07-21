#include "notifications.h"

#include <base/system.h>

#if defined(CONF_PLATFORM_MACOS)
// Code is in src/macos/notification.mm.
void NotificationsNotifyMacOsInternal(const char *pTitle, const char *pMessage);
#elif defined(CONF_FAMILY_UNIX) && !defined(CONF_PLATFORM_ANDROID) && !defined(CONF_PLATFORM_HAIKU) && !defined(CONF_PLATFORM_EMSCRIPTEN)
#include <libnotify/notify.h>
#define NOTIFICATIONS_USE_LIBNOTIFY
#elif defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;
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
	HINSTANCE LibShell32 = LoadLibraryW(L"shell32.dll");
	if(!LibShell32)
		return;

	auto SetAppUserModelProc = GetProcAddress(LibShell32, "SetCurrentProcessExplicitAppUserModelID");
	if(SetAppUserModelProc == nullptr)
	{
		FreeLibrary(LibShell32);
		return;
	}
#ifdef __MINGW32__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
	auto SetCurrentProcessExplicitAppUserModelID = reinterpret_cast<HRESULT(WINAPI *)(PCWSTR)>(SetAppUserModelProc);
#ifdef __MINGW32__
#pragma GCC diagnostic pop
#endif
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

	HRESULT Hr = S_OK;

	ComPtr<IXmlDocument> pAnswer;
	Hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), &pAnswer);
	if(FAILED(Hr))
		return;

	ComPtr<IXmlDocumentIO> pDocIo;
	Hr = pAnswer.As(&pDocIo);
	if(FAILED(Hr))
		return;

	Hr = pDocIo->LoadXml(HStringReference(windows_utf8_to_wide(aXml).c_str()).Get());
	if(FAILED(Hr))
		return;

	ComPtr<IXmlDocument> pDoc;
	Hr = pAnswer.CopyTo(&pDoc);
	if(FAILED(Hr))
		return;

	ComPtr<IToastNotificationManagerStatics> pToastStatics;
	Hr = Windows::Foundation::GetActivationFactory(
		HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
		&pToastStatics);
	if(FAILED(Hr))
		return;

	ComPtr<IToastNotifier> pNotifier;
	Hr = pToastStatics->CreateToastNotifierWithId(HStringReference(windows_utf8_to_wide(m_aAppName).c_str()).Get(), &pNotifier);
	if(FAILED(Hr))
		return;

	ComPtr<IToastNotificationFactory> pFactory;
	Hr = Windows::Foundation::GetActivationFactory(
		HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
		&pFactory);
	if(FAILED(Hr))
		return;

	ComPtr<IToastNotification> pToast;
	Hr = pFactory->CreateToastNotification(pDoc.Get(), &pToast);
	if(FAILED(Hr))
		return;

	FILETIME Ft;
	GetSystemTimeAsFileTime(&Ft);
	ULONGLONG Time = (((ULONGLONG)Ft.dwHighDateTime) << 32) | Ft.dwLowDateTime;
	Time += 10ULL * 10'000'000ULL; // 10 seconds

	DateTime Expiration;
	Expiration.UniversalTime = (INT64)Time;

	ComPtr<IPropertyValueStatics> pPropStatics;
	Hr = Windows::Foundation::GetActivationFactory(
		HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(),
		pPropStatics.ReleaseAndGetAddressOf());
	if(FAILED(Hr))
		return;

	ComPtr<IInspectable> pInspectable;
	Hr = pPropStatics->CreateDateTime(Expiration, &pInspectable);
	if(FAILED(Hr))
		return;

	ComPtr<IReference<DateTime>> pDateTime;
	Hr = pInspectable.As(&pDateTime);
	if(FAILED(Hr))
		return;

	Hr = pToast->put_ExpirationTime(pDateTime.Get());
	if(FAILED(Hr))
		return;

	Hr = pNotifier->Show(pToast.Get());
	if(FAILED(Hr))
		return;
#endif
}

INotifications *CreateNotifications()
{
	return new CNotifications();
}
