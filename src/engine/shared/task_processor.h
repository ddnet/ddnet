#ifndef ENGINE_SHARED_TASK_PROCESSOR_H
#define ENGINE_SHARED_TASK_PROCESSOR_H

#include <base/system.h>
#include <base/tl/threading.h>

#include <engine/shared/ReaderWriterQueue.h>

#include <atomic>
#include <memory>

template<typename TTask, typename TResult>
class ITaskProcessor
{
public:
	virtual ~ITaskProcessor() = default;
	virtual std::unique_ptr<TResult> ProcessTask(TTask &Task) = 0; // NOLINT(portability-template-virtual-member-function)
};

template<typename TTask, typename TResult>
class CTaskProcessor
{
public:
	using TaskType = TTask;
	using ResultType = TResult;
	using ProcessorType = ITaskProcessor<TaskType, ResultType>;

	explicit CTaskProcessor(ProcessorType &Processor) :
		m_Processor(Processor) {}
	~CTaskProcessor()
	{
		Stop();
	}

	void Start(const char *pThreadName)
	{
		if(m_pThread)
			return;

		m_Active.store(true, std::memory_order_release);
		m_pThread = thread_init(WorkerThread, this, pThreadName);
	}

	void Stop()
	{
		if(!m_pThread)
			return;

		m_Active.store(false, std::memory_order_release);
		m_Semaphore.Signal();
		thread_wait(m_pThread);
		m_pThread = nullptr;
	}

	void EnqueueTask(std::unique_ptr<TaskType> pTask)
	{
		if(!pTask)
			return;

		m_TaskQueue.enqueue(std::move(pTask));
		m_Semaphore.Signal();
	}

	void EnqueueResult(std::unique_ptr<ResultType> pResult)
	{
		if(!pResult)
			return;

		m_ResultQueue.enqueue(std::move(pResult));
	}

	bool TryDequeueResult(std::unique_ptr<ResultType> &pResult)
	{
		return m_ResultQueue.try_dequeue(pResult);
	}

private:
	static void WorkerThread(void *pUserData)
	{
		auto *pSelf = static_cast<CTaskProcessor *>(pUserData);
		pSelf->RunWorker();
	}

	void RunWorker()
	{
		while(true)
		{
			m_Semaphore.Wait();

			std::unique_ptr<TaskType> pTask;
			while(m_TaskQueue.try_dequeue(pTask))
			{
				auto pResult = m_Processor.ProcessTask(*pTask);
				if(pResult)
					m_ResultQueue.enqueue(std::move(pResult));
			}

			if(!m_Active.load(std::memory_order_acquire))
				break;
		}
	}

	ProcessorType &m_Processor;
	CSemaphore m_Semaphore;
	std::atomic_bool m_Active{false};
	void *m_pThread{};
	moodycamel::ReaderWriterQueue<std::unique_ptr<TaskType>> m_TaskQueue;
	moodycamel::ReaderWriterQueue<std::unique_ptr<ResultType>> m_ResultQueue;
};

#endif
