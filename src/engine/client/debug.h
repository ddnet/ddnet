#ifndef ENGINE_CLIENT_DEBUG_H
#define ENGINE_CLIENT_DEBUG_H

#include <string>
#include <list>

#include <csignal>
#include <engine/storage.h>

extern void *main_thread_handle;
inline bool is_thread()
{
	return thread_get_current() != main_thread_handle;
}

class CCallstack
{
	friend class CDebugger;
	std::list <std::string> m_CallStack;

public:
	void CallstackAdd(const char *file, int line, const char *function);

};

void signalhandler(int sig);

//CCallstack gDebuggingInfo;
extern CCallstack gDebugInfo;

class CDebugger
{
	MACRO_ALLOC_HEAP()
	//void (*signal(int sig, void (*func)(int)))(int);
	//static void signalhandler(int sig);

public:
	CDebugger();
	static void signalhandler_ex(int sig);
	static void make_crashreport(int sig, const char *pFilePath, char *aOutMsgBoxTitle, unsigned TitleBufferSize, char *aOutMsgBoxMsg, unsigned MsgBufferSize);

	static IStorage *m_pStorage;

	static void SetStaticData(IStorage *pStorage)
	{
		m_pStorage = pStorage;
	}

public:

private:
	void RegisterSignals();

};

#if defined(CONF_FAMILY_UNIX) && defined(FEATURE_DEBUGGER) && !defined(CONF_DEBUG)
#define CALLSTACK_ADD() if(!is_thread()) gDebugInfo.CallstackAdd(__FILE__, __LINE__, __FUNCTION__)
#else
#define CALLSTACK_ADD() ;;
#endif


#endif
