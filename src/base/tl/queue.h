#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

#include "threading.h"

class QueueDone : public std::runtime_error
{
	using std::runtime_error::runtime_error;
};

template <
	typename T,
	typename SPtr = std::unique_ptr<T>,
	typename Container = std::queue<SPtr>>
class queue
{
public:

	queue() = default;

	/* copyconstruction only works if the smartpointer SPtr used is also
	 * copyconstructible
	 * it is recommended to set SPtr to std::shared_ptr<T> to enable
	 * copyconstruction.
	 * Please note that the stored objects are not copied but only
	 * their lifetime is possibly prolonged.
	 */
	queue(const queue &Q)
	{
		auto Lck = Q.GetLock();
		m_Queue = Q.m_Queue;
		m_Finish = Q.m_Finish;
	}
	queue(queue &&) = delete;

	queue &operator=(const queue &Q)
	{
		if(this != &Q)
		{
			auto Lck = Q.GetLock();
			m_Queue = Q.m_Queue;
			m_Finish = Q.m_Finish;
		}
		return *this;
	}

	/* Lock the queue so the current thread is granted exclusive access and
	 * return the Lock.
	 *
	 * Note:
	 * The returned Lock shall never be discared as once it is destroyed (out of
	 * scope) the Lock will be released.
	 *
	 * In case this is used in combination with Pop it has to be ensured Pop
	 * will not block otherwise this results in a deadlock. Thus it is
	 * recommended to always check if Empty() == false and only then proceed to
	 * call Pop.
	 */
	std::unique_lock<std::recursive_mutex> GetLock() const
	{
		return m_CV.GetLock();
	}

	/* The same method as above with an additional option for the underlying
	 * Lock.
	 *
	 * Options are:
	 * std::adopt_lock
	 * std::defer_lock
	 * std::try_lock
	 *
	 * This can be useful if it is required to call Lock.lock() at a later time
	 * for example in combination with another lock via std::lock.
	 */
	template <typename LO>
	std::unique_lock<std::recursive_mutex> GetLock(LO LockOpt) const
	{
		return m_CV.GetLock(LockOpt);
	}

	typename Container::reference Front()
	{
		return m_Queue.front();
	}

	typename Container::const_reference Front() const
	{
		return m_Queue.front();
	}

	typename Container::reference Back()
	{
		return m_Queue.back();
	}

	typename Container::const_reference Back() const
	{
		return m_Queue.back();
	}

	bool Empty() const
	{
		return m_Queue.empty();
	}

	size_t Size() const
	{
		return m_Queue.size();
	}

	/* Notify the queue that after the call to this method no further elements
	 * will be added.
	 *
	 * If Emplace is called after Finish has been called QueueDone will be
	 * thrown.
	 * The same goes for Pop, so one can catch this Exception to gracefully stop
	 * waiting for further elements to arrive.
	 */
	void Finish()
	{
		auto Lck = m_CV.GetLock();
		m_Finish = true;
		m_Queue.emplace(nullptr);
		Lck.unlock();
		m_CV.NotifyAll();
	}

	// Return if Finish has been called.
	bool Finishing()
	{
		auto Lck = m_CV.GetLock();
		return m_Finish;
	}

	/* Construct an Object of Type T on the heap and pass args... to its
	 * constructor then append it to the queue.
	 *
	 * Note:
	 * throws QueueDone if called after Finish has been called.
	 */
	template <typename... Args>
	void Emplace(Args &&... args)
	{
		auto Lck = m_CV.GetLock();
		if(m_Finish)
			throw QueueDone("Can not add items to a queue on which Finish has "
							"been called.");
		m_Queue.emplace(new T(std::forward<Args>(args)...));
		// explicitly unlock so a waiting thread does not have to wait on the
		// lock
		Lck.unlock();
		m_CV.NotifyOne();
	}

	/* Pop an Element from the queue and return it wrapped in a smartpointer (by
	 * default std::unique_ptr).
	 *
	 * Note:
	 * If the queue is currently empty this blocks until new elements are added
	 * (and have not been popped by another thread) or Finish has been called.
	 *
	 * If Finish has been called and the queue is empty this throws QueueDone.
	 */
	typename Container::value_type Pop()
	{
		auto Lck = m_CV.GetLock();
		m_CV.Wait(Lck, [this] { return !m_Queue.empty(); });
		typename Container::value_type Ptr(std::move(m_Queue.front()));
		if(!Ptr)
			throw QueueDone("Reached end of this queue.");
		m_Queue.pop();
		return Ptr;
	}

private:
	Container m_Queue;

	// modifying the CV does not modify the actual content of the queue
	mutable condition_variable<std::recursive_mutex> m_CV;

	bool m_Finish = false;
};
