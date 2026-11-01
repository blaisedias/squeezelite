#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include "squeezelite.h"

//====================
// copied from output_vis.c
#define VIS_BUF_SIZE 16384
#define VIS_LOCK_NS  1000000 // ns to wait for vis wrlock
                             //
static struct vis_t {
	pthread_rwlock_t rwlock;
	u32_t buf_size;
	u32_t buf_index;
	bool running;
	u32_t rate;
	time_t updated;
	s16_t buffer[VIS_BUF_SIZE];
} *vis_mmap = NULL;
//

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
    puts("");

	struct timespec ts;
	printf("sizeof(timespec)            : %u\n", (unsigned)sizeof(struct timespec));
	printf("sizeof(timespec.tv_sec)     : %u\n", (unsigned)sizeof(ts.tv_sec));
	printf("sizeof(timespec.tv_nsec)    : %u\n", (unsigned)sizeof(ts.tv_nsec));
	printf("sizeof(time_t)              : %u\n", (unsigned)sizeof(time_t));
	printf("sizeof(struct vis_t)        : %u\n", (unsigned)sizeof(*vis_mmap));
    puts("");

	u32_t max_u32 = ~0;
	u64_t max_u64 = ~0;
	printf("u32_t max                   : %u\n", max_u32);
	printf("u32_t time                  : %u\n", gettime_ms());
    float s = max_u32;
    s /= 1000;          // milli seconds
    s /= (24*60*60);    // seconds in a day
	printf("u32_t max days              : %f\n", s);
    puts("");

	printf("u64_t max                   : %" PRIu64 "\n", max_u64);
	printf("u64_t time                  : %" PRIu64 "\n", gettime_ms64());
    s = max_u64;
    s /= 1000;          // milli seconds
    s /= (24*60*60);    // seconds in a day
	printf("u64_t max                   : %f\n", s);
	return 0;
}
