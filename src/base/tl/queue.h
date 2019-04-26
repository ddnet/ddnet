#ifndef BASE_TL_QUEUE_H
#define BASE_TL_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>


template<typename T>
class Queue
{
private:
	std::mutex m_Mutex;
	std::condition_variable m_CV;
	std::queue<T> m_Queue;

public:
	void push(const T &pItem)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Queue.push(pItem);
		lock.unlock();
		m_CV.notify_one();
	}

	void push(T &&ppItem)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Queue.push(std::move(ppItem));
		lock.unlock();
		m_CV.notify_one();
	}

	T pop()
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		while(m_Queue.empty())
		{
			m_CV.wait(lock);
		}
		T item = m_Queue.front();
		m_Queue.pop();
		return item;
	}

	void pop(T& item)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		while (m_Queue.empty())
		{
			m_CV.wait(lock);
		}
		item = m_Queue.front();
		m_Queue.pop();
	}
};


#endif //BASE_TL_QUEUE_H
