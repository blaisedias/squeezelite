#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include "squeezelite.h"
//
// compile with
// gcc -Wall -fPIC -O2 -fcommon -std=gnu99 time_exp.c -o time_exp
// OR
// gcc -Wall -fPIC -O2 -fcommon -std=gnu99 -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 time_exp.c -o time_exp64
// 

// ======================
// copied from utils.c
// clock
u32_t gettime_ms(void) {
#if WIN
	return GetTickCount();
#else
#if LINUX || FREEBSD
	struct timespec ts;
#ifdef CLOCK_MONOTONIC
	if (!clock_gettime(CLOCK_MONOTONIC, &ts)) {
#else
	if (!clock_gettime(CLOCK_REALTIME, &ts)) {
#endif
		return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}
#endif
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}
// ======================

u64_t gettime_ms64(void) {
#if WIN
	return GetTickCount();
#else
#if LINUX || FREEBSD
	struct timespec ts;
#ifdef CLOCK_MONOTONIC
	if (!clock_gettime(CLOCK_MONOTONIC, &ts)) {
#else
	if (!clock_gettime(CLOCK_REALTIME, &ts)) {
#endif
		u64_t v_sec = ts.tv_sec;
		u64_t v_nsec = ts.tv_nsec;
		return v_sec * 1000 + v_nsec / 1000000;
	}
#endif
	struct timeval tv;
	gettimeofday(&tv, NULL);
	u64_t v_sec = tv.tv_sec;
	u64_t v_usec = tv.tv_usec;
	return v_sec * 1000 + v_usec / 1000;
#endif
}

int main(void) {
	printf("sizeof(short unsigned)      : %u\n", (unsigned)sizeof(short unsigned));
	printf("sizeof(unsigned)            : %u\n", (unsigned)sizeof(unsigned));
	printf("sizeof(long unsigned)       : %u\n", (unsigned)sizeof(long unsigned));
	printf("sizeof(long long unsigned)  : %u\n", (unsigned)sizeof(long long unsigned));

	struct timespec ts;
	printf("sizeof(timespec)            : %u\n", (unsigned)sizeof(struct timespec));
	printf("sizeof(timespec.tv_sec)     : %u\n", (unsigned)sizeof(ts.tv_sec));
	printf("sizeof(timespec.tv_nsec)    : %u\n", (unsigned)sizeof(ts.tv_nsec));

	u32_t max = ~1;
	printf("u32_t max                   : %u\n", max);
	printf("u32_t time                  : %u\n", gettime_ms());
	printf("u64_t time                  : %" PRIu64 "\n", gettime_ms64());

	return 0;
}
