#ifndef BASE_TL_MPSC_H
#define BASE_TL_MPSC_H

#include "threading.h"

#include <atomic>
#include <mutex>
#include <queue>

// simple thread safe multiple producer, single consumer queue
template<class T, int size>
class CMpsc
{
public:
	CMpsc()
		: m_Head(0),
		  m_Tail(0),
		  m_SignalElems(new semaphore()),
		  m_SendMutex(new std::mutex())
	{
	}

	void Send(T Task)
	{
		std::lock_guard<std::mutex> lock(*m_SendMutex);
		m_Queue[m_Head++] = std::move(Task);
		m_Head %= sizeof(m_Queue) / sizeof(m_Queue[0]);
		m_SignalElems->signal();
	}

	T Recv()
	{
		m_SignalElems->wait();
		T pThreadData = std::move(m_Queue[m_Tail++]);
		m_Tail %= sizeof(m_Queue) / sizeof(m_Queue[0]);
		return pThreadData; // guaranteed to be move semantic
	}

	// non-blocking; given pointer has to be allocated
	// only written to if true is returned
	bool TryRecv(T *pTask)
	{
		if(m_SignalElems->trywait())
		{
			*pTask = std::move(m_Queue[m_Tail++]);
			m_Tail %= sizeof(m_Queue) / sizeof(m_Queue[0]);
			return true;
		}
		return false;
	}

private:
	int m_Head;
	int m_Tail;
	std::unique_ptr<semaphore> m_SignalElems;
	std::unique_ptr<std::mutex> m_SendMutex;
	T m_Queue[size];
};

#endif // BASE_TL_MPSC_H
