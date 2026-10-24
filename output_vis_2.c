/* 
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *      Ralph Irving 2015-2026, ralph_irving@hotmail.com
 *      Blaise Dias  2026, https://github.com/blaisedias
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Export audio samples for visualizer process (16 bit only best endevours)

#include "squeezelite.h"

#if VISEXPORT_2

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <assert.h>
#if OSX
#include <mach/clock.h>
#include <mach/mach.h>

//static int pthread_rwlock_timedwrlock( pthread_rwlock_t * restrict rwlock, const struct timespec * restrict abs_timeout )
//{
//    ( void )rwlock;
//    ( void )abs_timeout;
//    
//    return 0;
//}
#endif

#define VIS_BUF_SIZE 16384
#define VIS_LOCK_NS  1000000 // ns to wait for vis wrlock

#define MIN(a,b) (((a)<=(b))?(a):(b))

static struct vis_t {
	pthread_rwlock_t rwlock;
	u32_t buf_size;
	u32_t buf_index;
	bool running;
	u32_t rate;
	time_t updated;
	s16_t buffer[VIS_BUF_SIZE];
} *vis_mmap = NULL;

static char vis_shm_path[40];
static int vis_fd = -1;

static log_level loglevel;

extern int64_t get_micro_seconds();
extern int64_t get_milli_seconds();

static pthread_t samples_thread;
static sem_t samples_sem;
static s32_t samples_buf[VIS_BUF_SIZE];
static volatile unsigned samples_wr_index = 0;
static volatile unsigned samples_rd_index = 0;
static volatile u32_t current_replay_gain = 0;
static volatile u32_t current_sample_rate = 0;
static volatile bool running;
static volatile bool keep_running = true;

static volatile unsigned long long total_wr_samples=0;
static volatile unsigned long long total_rd_samples=0;

unsigned samples_count;

int64_t get_micro_seconds() {
	int64_t micros;
	struct timespec  ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
	micros = (ts.tv_sec*1000000);
	micros += (ts.tv_nsec/1000);
	return micros;
}

int64_t get_milli_seconds() {
	return get_micro_seconds()/1000;
}

static void *samples_thread_fn(void *arg) {
	static unsigned prev_rd_sample_count = 0;
	for(; keep_running;) {
		sem_wait(&samples_sem);
		if (vis_mmap) {
			pthread_rwlock_wrlock(&vis_mmap->rwlock);
			unsigned index = vis_mmap->buf_index;
			unsigned rd_sample_count = __atomic_load_n(&samples_count, __ATOMIC_ACQUIRE);
			if (rd_sample_count > VIS_BUF_SIZE) {
				// sample buffer drain underflow, attempt to resync
				fprintf(stderr," RD_XS: %u\n", rd_sample_count);
				rd_sample_count = VIS_BUF_SIZE;
				samples_rd_index = (samples_wr_index + 1) % VIS_BUF_SIZE;
			}
			for(unsigned ix = 0; ix < rd_sample_count; ++ix) {
				if (!current_replay_gain) {
					vis_mmap->buffer[index] = samples_buf[samples_rd_index] >> 16;
				} else {
					vis_mmap->buffer[index] = gain(samples_buf[samples_rd_index], current_replay_gain) >> 16;
				}
				samples_rd_index = (samples_rd_index + 1) % VIS_BUF_SIZE;
				index = (index + 1) % VIS_BUF_SIZE;
			}
			total_rd_samples += rd_sample_count;
			vis_mmap->buf_index = index;
			vis_mmap->running = running;
			pthread_rwlock_unlock(&vis_mmap->rwlock);
			__atomic_sub_fetch(&samples_count, rd_sample_count, __ATOMIC_ACQ_REL);
#ifdef DEBUG_SAMPLING
//fprintf(stderr,"(%u,%u)", samples_wr_index, samples_rd_index);
fprintf(stderr,"%u>\n", rd_sample_count); fflush(stdout);
#endif
			if(prev_rd_sample_count && rd_sample_count && prev_rd_sample_count < rd_sample_count) {
				fprintf(stderr, "rds: %5u was %5u  indices:wr= %5u rd= %5u tot:wr= %llu rd= %llu\n",
						rd_sample_count, prev_rd_sample_count,
						samples_wr_index, samples_rd_index,
						total_wr_samples, total_rd_samples);
			}
			prev_rd_sample_count = rd_sample_count;
		}
	}
	return NULL;
}

void _vis_2_export(struct buffer *outputbuf, struct outputstate *output, frames_t out_frames, bool silence) {
	if (vis_mmap) {
		if (silence) {
			if (running) {
				running = false;
				sem_post(&samples_sem);
			}
		} else {
			unsigned sample_count = out_frames * 2;
			uint8_t* readp = outputbuf->readp;
			if (sample_count > VIS_BUF_SIZE) {
				// sample buffer overflow
				// the sample count is larger than the buffer, skip forward
				// skip forward and only pass the newest samples
				fprintf(stderr," WR_XS: %u\n", sample_count);
				readp += sizeof(samples_buf[0]) * (sample_count - VIS_BUF_SIZE);
				sample_count = VIS_BUF_SIZE;
			}
			unsigned avail = VIS_BUF_SIZE - samples_wr_index;
#ifdef DEBUG_SAMPLING
fprintf(stderr,"<%u ", sample_count);
#endif
			if (sample_count > avail) {
				memcpy(&samples_buf[samples_wr_index], readp, sizeof(samples_buf[0])*avail);
				readp += sizeof(samples_buf[0]) *avail;
				sample_count -= avail;
				samples_wr_index = 0;
				total_wr_samples += avail;
				__atomic_add_fetch(&samples_count, avail, __ATOMIC_ACQ_REL);
			}
			memcpy(&samples_buf[samples_wr_index], readp, sizeof(samples_buf[0])*sample_count);
			samples_wr_index += sample_count;
			current_replay_gain = output->current_replay_gain;
			current_sample_rate = output->current_sample_rate;
			running = true;
			total_wr_samples += sample_count;
			__atomic_add_fetch(&samples_count, sample_count, __ATOMIC_ACQ_REL);
			sem_post(&samples_sem);
#ifdef DEBUG_SAMPLING
//fprintf(stderr," (%u,%u) ", samples_wr_index, samples_rd_index);
#endif
		}
	}
}

void _vis_2_stop(void) {
	if (vis_mmap) {
		running = false;
		sem_post(&samples_sem);
		pthread_rwlock_wrlock(&vis_mmap->rwlock);
		vis_mmap->running = false;
		pthread_rwlock_unlock(&vis_mmap->rwlock);
	 	keep_running = false;
		sem_post(&samples_sem);
		if (samples_thread) {
			pthread_join(samples_thread, NULL);
		}
	}
	sem_destroy(&samples_sem);
}

void output_vis_2_init(log_level level, u8_t *mac) {
	loglevel = level;

	if (sem_init(&samples_sem, 0, 0) == -1 ){
		fprintf(stderr, "failed to initialise the semaphore\n");
		exit(EXIT_FAILURE);
	}

	sprintf(vis_shm_path, "/squeezelite-%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	mode_t old_mask = umask(000); // allow any user to read our shm when created

#if OSX
	/* ftruncate on MacOS only works on the initial segment creation */
	shm_unlink(vis_shm_path);
#endif	
	vis_fd = shm_open(vis_shm_path, O_CREAT | O_RDWR, 0666);
	if (vis_fd != -1) {
		if (ftruncate(vis_fd, sizeof(struct vis_t)) == 0) {
			vis_mmap = (struct vis_t *)mmap(NULL, sizeof(struct vis_t), PROT_READ | PROT_WRITE, MAP_SHARED, vis_fd, 0);
		}
	}
	
	if (vis_mmap > 0) {
		pthread_rwlockattr_t attr;
		pthread_rwlockattr_init(&attr);
		pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		pthread_rwlock_init(&vis_mmap->rwlock, &attr);
		vis_mmap->buf_size = VIS_BUF_SIZE;
		vis_mmap->running = false;
		vis_mmap->rate = 44100;
		pthread_rwlockattr_destroy(&attr);
		LOG_INFO("opened visulizer shared memory as %s", vis_shm_path);
	} else {
		LOG_WARN("unable to open visualizer shared memory");
		vis_mmap = NULL;
	}

	umask(old_mask);
	// start samples processing thread
	keep_running = true;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + OUTPUT_THREAD_STACK_SIZE);
	pthread_create(&samples_thread, &attr, samples_thread_fn, NULL);
	pthread_attr_destroy(&attr);
}
#endif // VISEXPORT_2
