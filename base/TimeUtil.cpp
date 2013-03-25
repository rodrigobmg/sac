#include "TimeUtil.h"
#include <time.h>
#ifdef DARWIN
#include <sys/time.h>
#elif defined(WINDOWS)
#include "Mmsystem.h"
#undef ERROR
#endif

#if defined(WINDOWS) && defined(ERROR)
#undef ERROR
#endif
#include "Log.h"

#if defined(EMSCRIPTEN) || defined(DARWIN)
	struct timeval TimeUtil::startup_time;
#elif defined(LINUX)
	struct timespec TimeUtil::startup_time;
#elif defined(WINDOWS)
	__int64 TimeUtil::startup_time;
	double TimeUtil::frequency;
#endif

void TimeUtil::Init() {
#if defined(EMSCRIPTEN) || defined(DARWIN)
    gettimeofday(&startup_time, 0);
#elif defined(LINUX)
	clock_gettime(CLOCK_MONOTONIC, &startup_time);    
#elif defined(WINDOWS)
    timeBeginPeriod(1);
	QueryPerformanceCounter((LARGE_INTEGER*)&startup_time);
	__int64 invertfrequency;
	QueryPerformanceFrequency((LARGE_INTEGER*)&invertfrequency);
	frequency = 1.0 / invertfrequency;
#endif
}

#if defined(LINUX)
static inline float timeconverter(const struct timespec & tv) {
	return tv.tv_sec + (float)(tv.tv_nsec) / 1000000000.0f;
#elif defined(EMSCRIPTEN) || defined(DARWIN)
static inline float timeconverter(const struct timeval & tv) {
    return (tv.tv_sec + tv.tv_usec / 1000000.0f);
#else
static inline float timeconverter(float tv) {
	return (tv);
#endif
}


#if defined(LINUX)
static inline void sub(struct timespec& tA, const struct timespec& tB)
{
    if ((tA.tv_nsec - tB.tv_nsec) < 0) {
        tA.tv_sec = tA.tv_sec - tB.tv_sec - 1;
        tA.tv_nsec = 1000000000 + tA.tv_nsec - tB.tv_nsec;
    } else {
        tA.tv_sec = tA.tv_sec - tB.tv_sec;
        tA.tv_nsec = tA.tv_nsec - tB.tv_nsec;
    }
}
#endif

float TimeUtil::GetTime() {
    #if defined(LINUX)
		struct timespec tv;
		if (clock_gettime(CLOCK_MONOTONIC, &tv) != 0) {
        LOGF("clock_gettime failure")
		}
		sub(tv, startup_time);
    #elif defined(EMSCRIPTEN) || defined(DARWIN)
		struct timeval tv;
		gettimeofday(&tv, 0);
		timersub(&tv, &startup_time, &tv);
	#elif defined(WINDOWS)
		__int64 intv;
		QueryPerformanceCounter((LARGE_INTEGER*)&intv);
        intv -= startup_time;
		float tv = intv * frequency;
    #endif
	return timeconverter(tv);
}

void TimeUtil::Wait(float waitInSeconds) {
       LOGW_IF(waitInSeconds >= 1, "TODO, handle sleep request >= 1 s");
       float before = GetTime();
       float delta = 0;
       while (delta < waitInSeconds) {
#if defined(LINUX) || defined(DARWIN)
           struct timespec ts;
           ts.tv_sec = 0;
           ts.tv_nsec = (waitInSeconds - delta) * 1000000000LL;
           nanosleep(&ts, 0);
#else
           // Of course using Sleep is bad, but hey...
           Sleep((waitInSeconds - delta) * 1000);
#endif
           delta = GetTime() - before;
       }
}
