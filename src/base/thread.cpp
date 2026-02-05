/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "thread.h"

#include "dbg.h"
#include "windows.h"

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#include <sched.h>

#include <cstdlib>

#if defined(CONF_PLATFORM_MACOS)
#if defined(__MAC_10_10) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_10
#include <pthread/qos.h>
#endif
#endif

#if defined(CONF_PLATFORM_EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif

#elif defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#else
#error NOT IMPLEMENTED
#endif

struct THREAD_RUN
{
	void (*threadfunc)(void *);
	void *u;
};

#if defined(CONF_FAMILY_UNIX)
static void *thread_run(void *user)
#elif defined(CONF_FAMILY_WINDOWS)
static unsigned long __stdcall thread_run(void *user)
#else
#error not implemented
#endif
{
#if defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(false);
#endif
	struct THREAD_RUN *data = (THREAD_RUN *)user;
	void (*threadfunc)(void *) = data->threadfunc;
	void *u = data->u;
	free(data);
	threadfunc(u);
	return 0;
}

void *thread_init(void (*threadfunc)(void *), void *u, const char *name)
{
	struct THREAD_RUN *data = (THREAD_RUN *)malloc(sizeof(*data));
	data->threadfunc = threadfunc;
	data->u = u;
#if defined(CONF_FAMILY_UNIX)
	{
		pthread_attr_t attr;
		dbg_assert(pthread_attr_init(&attr) == 0, "pthread_attr_init failure");
#if defined(CONF_PLATFORM_MACOS) && defined(__MAC_10_10) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_10
		dbg_assert(pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, 0) == 0, "pthread_attr_set_qos_class_np failure");
#endif
		pthread_t id;
		dbg_assert(pthread_create(&id, &attr, thread_run, data) == 0, "pthread_create failure");
#if defined(CONF_PLATFORM_EMSCRIPTEN)
		// Return control to the browser's main thread to allow the pthread to be started,
		// otherwise we deadlock when waiting for a thread immediately after starting it.
		emscripten_sleep(0);
#endif
		return (void *)id;
	}
#elif defined(CONF_FAMILY_WINDOWS)
	HANDLE thread = CreateThread(nullptr, 0, thread_run, data, 0, nullptr);
	dbg_assert(thread != nullptr, "CreateThread failure");
	HMODULE kernel_base_handle;
	if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"KernelBase.dll", &kernel_base_handle))
	{
		// Intentional
#ifdef __MINGW32__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
		auto set_thread_description_function = reinterpret_cast<HRESULT(WINAPI *)(HANDLE, PCWSTR)>(GetProcAddress(kernel_base_handle, "SetThreadDescription"));
#ifdef __MINGW32__
#pragma GCC diagnostic pop
#endif
		if(set_thread_description_function)
			set_thread_description_function(thread, windows_utf8_to_wide(name).c_str());
	}
	return thread;
#else
#error not implemented
#endif
}

void thread_wait(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(pthread_join((pthread_t)thread, nullptr) == 0, "pthread_join failure");
#elif defined(CONF_FAMILY_WINDOWS)
	dbg_assert(WaitForSingleObject((HANDLE)thread, INFINITE) == WAIT_OBJECT_0, "WaitForSingleObject failure");
	dbg_assert(CloseHandle(thread), "CloseHandle failure");
#else
#error not implemented
#endif
}

void thread_yield()
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(sched_yield() == 0, "sched_yield failure");
#elif defined(CONF_FAMILY_WINDOWS)
	Sleep(0);
#else
#error not implemented
#endif
}

void thread_detach(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	dbg_assert(pthread_detach((pthread_t)thread) == 0, "pthread_detach failure");
#elif defined(CONF_FAMILY_WINDOWS)
	dbg_assert(CloseHandle(thread), "CloseHandle failure");
#else
#error not implemented
#endif
}

void thread_init_and_detach(void (*threadfunc)(void *), void *u, const char *name)
{
	void *thread = thread_init(threadfunc, u, name);
	thread_detach(thread);
}
