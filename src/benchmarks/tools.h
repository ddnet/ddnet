#ifndef BENCHMARK_TOOLS_H
#define BENCHMARK_TOOLS_H

#include <base/system.h>

class CTimer
{
public:
	CTimer()
	{
		Reset();
	}
	
	CTimer(int64 StartTime) : m_StartTime(StartTime) { }

	int64 GetElapsed(int64 CurrentTime)
	{
		return CurrentTime - m_StartTime;
	}

	int64 GetElapsed()
	{
		return GetElapsed(time_get());
	}
	
	void Reset()
	{
		m_StartTime = time_get();
	}

	void PrintElapsed(const char *pName, int64 CurrentTime)
	{
		double Elapsed = GetElapsed(CurrentTime) / (double)time_freq();
		dbg_msg("benchmark", "%s: %.2fms", pName, Elapsed * 1000);
	}

	void PrintElapsed(const char *pName)
	{
		PrintElapsed(pName, time_get());
	}

private:
	int64 m_StartTime;
};

class CFrameLoopAnalyser
{
public:
	CFrameLoopAnalyser(int Duration, int MaxFPS) :
		m_FrameCounter(0),
		m_StartTime(-1),
		m_CurrentTime(-1),
		m_Duration(Duration)
	{
		m_aFrameTimes.hint_size(Duration*MaxFPS);
	}
	
	int64 GetFrameTime() const
	{
		return m_CurrentTime;
	}
	
	bool Continue()
	{
		// update frame time
		int64 NewTime = time_get();
		if(m_CurrentTime < 0)
		{
			m_StartTime = NewTime;
		}
		else
		{
			int64 TimeDiff = NewTime - m_CurrentTime;
			m_aFrameTimes.add(TimeDiff);
			m_FrameCounter++;
		}
		
		m_CurrentTime = NewTime;
		
		// check end of analyse
		{
			int64 AnalyseDuration = m_CurrentTime - m_StartTime;
			if(AnalyseDuration > time_freq() * m_Duration)
				return false;
		}
		
		return true;
	}
	
	void PrintReport()
	{
		double Time = (m_CurrentTime - m_StartTime) / (double)time_freq();
		double Fps = (double)(m_FrameCounter - 1) / Time;
		dbg_msg("benchmark", "result: %.2fms per frame (%.2f fps)", 1 / Fps * 1000, Fps);

		sort(m_aFrameTimes.all());

		int NumLongestFrames = min(10, m_aFrameTimes.size());
		for(int i = 0; i < NumLongestFrames; i++)
		{
			double Time = m_aFrameTimes[m_aFrameTimes.size() - 1 - i] / (double)time_freq() * 1000;
			char aPrefix[6] = "";
			if(i != 0)
			{
				const char *pSuffix = "th";
				if(i == 1)
				{
					pSuffix = "nd";
				}
				else if(i == 2)
				{
					pSuffix = "rd";
				}
				str_format(aPrefix, sizeof(aPrefix), "%d%s ", i + 1, pSuffix);
			}
			dbg_msg("benchmark", "%slongest frame: %.2fms", aPrefix, Time);
		}
		dbg_msg("benchmark", "median frame: %.2fms", m_aFrameTimes[m_aFrameTimes.size() / 2] / (double)time_freq() * 1000);
	}

private:
	unsigned int m_FrameCounter;
	array<int64> m_aFrameTimes;
	int64 m_StartTime;
	int64 m_CurrentTime;
	int m_Duration;
};

#endif
