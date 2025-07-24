#pragma once

class SimpleLock
{
	enum
	{
		kFastSpinThreshold = 10000
	};

	volatile std::int32_t	threadID;	// 00
	volatile std::uint32_t	lockCount;	// 04

public:
	SimpleLock() : threadID(0), lockCount(0) {}

	void Lock(std::uint32_t pauseAttempts = 0) {
		std::int32_t myThreadID = GetCurrentThreadId();

		_mm_lfence();
		if (threadID == myThreadID)
		{
			InterlockedIncrement(&lockCount);
		}
		else
		{
			std::uint32_t attempts = 0;
			if (InterlockedCompareExchange(&lockCount, 1, 0))
			{
				do
				{
					++attempts;
					_mm_pause();
					if (attempts >= pauseAttempts) {
						std::uint32_t spinCount = 0;
						while (InterlockedCompareExchange(&lockCount, 1, 0))
							Sleep(++spinCount < kFastSpinThreshold ? 0 : 1);
						break;
					}
				} while (InterlockedCompareExchange(&lockCount, 1, 0));
				_mm_lfence();
			}

			threadID = myThreadID;
			_mm_sfence();
		}
	}
	void Release(void) {
		std::int32_t myThreadID = GetCurrentThreadId();

		_mm_lfence();
		if (threadID == myThreadID)
		{
			if (lockCount == 1)
			{
				threadID = 0;
				_mm_mfence();
				InterlockedCompareExchange(&lockCount, 0, 1);
			}
			else
			{
				InterlockedDecrement(&lockCount);
			}
		}
	}
};
static_assert(sizeof(SimpleLock) == 0x8);

class SimpleLocker
{
public:
	SimpleLocker(SimpleLock* dataHolder) { m_lock = dataHolder; m_lock->Lock(); }
	~SimpleLocker() { m_lock->Release(); }

protected:
	SimpleLock* m_lock;
};

template <typename T>
class SafeDataHolder
{
protected:
	mutable SimpleLock	m_lock;
public:
	T			m_data;

	void	Lock(void) { m_lock.Lock(); }
	void	Release(void) { m_lock.Release(); }

	template<typename T> friend class SafeDataLocker;
};
