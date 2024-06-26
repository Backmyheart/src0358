#define _GNU_SOURCE 

#include <endian.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <linux/futex.h>

static void sleep_ms(uint64_t ms)
{
	usleep(ms * 1000);
}

static uint64_t current_time_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
	exit(1);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void thread_start(void* (*fn)(void*), void* arg)
{
	pthread_t th;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 128 << 10);
	int i = 0;
	for (; i < 100; i++) {
		if (pthread_create(&th, &attr, fn, arg) == 0) {
			pthread_attr_destroy(&attr);
			return;
		}
		if (errno == EAGAIN) {
			usleep(50);
			continue;
		}
		break;
	}
	exit(1);
}

typedef struct {
	int state;
} event_t;

static void event_init(event_t* ev)
{
	ev->state = 0;
}

static void event_reset(event_t* ev)
{
	ev->state = 0;
}

static void event_set(event_t* ev)
{
	if (ev->state)
	exit(1);
	__atomic_store_n(&ev->state, 1, __ATOMIC_RELEASE);
	syscall(SYS_futex, &ev->state, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1000000);
}

static void event_wait(event_t* ev)
{
	while (!__atomic_load_n(&ev->state, __ATOMIC_ACQUIRE))
		syscall(SYS_futex, &ev->state, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0, 0);
}

static int event_isset(event_t* ev)
{
	return __atomic_load_n(&ev->state, __ATOMIC_ACQUIRE);
}

static int event_timedwait(event_t* ev, uint64_t timeout)
{
	uint64_t start = current_time_ms();
	uint64_t now = start;
	for (;;) {
		uint64_t remain = timeout - (now - start);
		struct timespec ts;
		ts.tv_sec = remain / 1000;
		ts.tv_nsec = (remain % 1000) * 1000 * 1000;
		syscall(SYS_futex, &ev->state, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0, &ts);
		if (__atomic_load_n(&ev->state, __ATOMIC_ACQUIRE))
			return 1;
		now = current_time_ms();
		if (now - start > timeout)
			return 0;
	}
}

struct thread_t {
	int created, call;
	event_t ready, done;
};

static struct thread_t threads[16];
static void execute_call(int call);
static int running;

static void* thr(void* arg)
{
	struct thread_t* th = (struct thread_t*)arg;
	for (;;) {
		event_wait(&th->ready);
		event_reset(&th->ready);
		execute_call(th->call);
		__atomic_fetch_sub(&running, 1, __ATOMIC_RELAXED);
		event_set(&th->done);
	}
	return 0;
}

static void loop(void)
{
	int i, call, thread;
	for (call = 0; call < 10; call++) {
		for (thread = 0; thread < (int)(sizeof(threads) / sizeof(threads[0])); thread++) {
			struct thread_t* th = &threads[thread];
			if (!th->created) {
				th->created = 1;
				event_init(&th->ready);
				event_init(&th->done);
				event_set(&th->done);
				thread_start(thr, th);
			}
			if (!event_isset(&th->done))
				continue;
			event_reset(&th->done);
			th->call = call;
			__atomic_fetch_add(&running, 1, __ATOMIC_RELAXED);
			event_set(&th->ready);
			event_timedwait(&th->done, 50);
			break;
		}
	}
	for (i = 0; i < 100 && __atomic_load_n(&running, __ATOMIC_RELAXED); i++)
		sleep_ms(1);
}

uint64_t r[4] = {0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff};

void execute_call(int call)
{
		intptr_t res = 0;
	switch (call) {
	case 0:
		res = syscall(__NR_socket, 0x10ul, 3ul, 0);
		if (res != -1)
				r[0] = res;
		break;
	case 1:
*(uint64_t*)0x20000040 = 0;
*(uint32_t*)0x20000048 = 0;
*(uint64_t*)0x20000050 = 0x20000000;
*(uint64_t*)0x20000000 = 0x20000080;
memcpy((void*)0x20000080, "\x48\x00\x00\x00\x10\x00\x05\x07\x00\x00\x00\x00\x00\x00\x00\x00\x5a\x52\x00\x00", 20);
*(uint32_t*)0x20000094 = -1;
memcpy((void*)0x20000098, "\x00\x00\x00\x00\x00\x00\x00\x00\x28\x00\x12\x00\x09\x00\x01\x00\x76\x65\x74\x68", 20);
*(uint64_t*)0x20000008 = 0x48;
*(uint64_t*)0x20000058 = 1;
*(uint64_t*)0x20000060 = 0;
*(uint64_t*)0x20000068 = 0;
*(uint32_t*)0x20000070 = 0;
		syscall(__NR_sendmsg, r[0], 0x20000040ul, 0ul);
		break;
	case 2:
		res = syscall(__NR_pipe, 0x20000080ul);
		if (res != -1) {
r[1] = *(uint32_t*)0x20000080;
r[2] = *(uint32_t*)0x20000084;
		}
		break;
	case 3:
		res = syscall(__NR_socket, 2ul, 2ul, 0);
		if (res != -1)
				r[3] = res;
		break;
	case 4:
		syscall(__NR_close, r[3]);
		break;
	case 5:
		syscall(__NR_socket, 0x10ul, 0x803ul, 0);
		break;
	case 6:
*(uint8_t*)0x20000040 = 0;
memcpy((void*)0x20000041, "rdma", 4);
*(uint8_t*)0x20000045 = 0x20;
		syscall(__NR_write, -1, 0x20000040ul, 6ul);
		break;
	case 7:
*(uint64_t*)0x200000c0 = 0;
*(uint32_t*)0x200000c8 = 0;
*(uint64_t*)0x200000d0 = 0x20000200;
*(uint64_t*)0x20000200 = 0x20000000;
memcpy((void*)0x20000000, "\x48\x00\x00\x00\x10\x00\x1f\xff\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00", 20);
*(uint32_t*)0x20000014 = -1;
memcpy((void*)0x20000018, "\x1f\x00\x00\xf5\xb2\xcc\xdc\x00\x28\x00\x12\x80\x0a\x00\x01\x00\x76\x78\x6c\x61\x6e\x00\x00\x00\x18\x00\x02\x80\x14\x00\x10", 31);
*(uint64_t*)0x20000208 = 3;
*(uint64_t*)0x200000d8 = 1;
*(uint64_t*)0x200000e0 = 0;
*(uint64_t*)0x200000e8 = 0;
*(uint32_t*)0x200000f0 = 0;
		syscall(__NR_sendmsg, -1, 0x200000c0ul, 0ul);
		break;
	case 8:
		syscall(__NR_write, r[2], 0x20000000ul, 0xfffffeccul);
		break;
	case 9:
		syscall(__NR_splice, r[1], 0ul, r[3], 0ul, 0x4ffe2ul, 0ul);
		break;
	}

}
int main(void)
{
	if (unshare(CLONE_NEWUSER)||unshare(CLONE_NEWNS)) {
        }
        if (unshare(CLONE_NEWNET)) {
        }

	syscall(__NR_mmap, 0x1ffff000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
	syscall(__NR_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x32ul, -1, 0ul);
	syscall(__NR_mmap, 0x21000000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
			loop();
	return 0;
}
