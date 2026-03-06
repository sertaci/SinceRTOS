#ifndef OS_H
#define OS_H

#include <stddef.h>
#include <stdint.h>

/* --- SINCERTOS KERNEL CONFIGURATION --- */
#define CONFIG_CPU_FREQ_HZ 80000000ULL
#define CONFIG_TICK_RATE_HZ 1000ULL

#define SRTOS_CYCLES_PER_TICK (CONFIG_CPU_FREQ_HZ / CONFIG_TICK_RATE_HZ) /* 80,000 */
#define SRTOS_CYCLES_PER_US (CONFIG_CPU_FREQ_HZ / 1000000ULL)			 /* 80 */

#define SRTOS_STACK_SIZE 2048
#define SRTOS_MAX_TASKS 6

/* --- TASK STATES --- */
#define SRTOS_STATE_READY 0
#define SRTOS_STATE_BLOCKED 1
#define SRTOS_STATE_WAIT_QUEUE 2
#define SRTOS_STATE_WAIT_MUTEX 3

extern volatile uint32_t system_ticks;
extern volatile int current_task;

typedef struct {
	uint32_t *sp;
	volatile uint32_t state;
	volatile uint32_t wake_tick;
	volatile int base_priority;
	volatile int priority;
	void *waiting_mutex;
	uint32_t padding[2];
} TCB_t;

extern volatile TCB_t task_list[SRTOS_MAX_TASKS];

/* --- MUTEX STRUCTURE --- */
typedef struct {
	volatile int locked;
	volatile int owner_task_id;
	// volatile int waiting_task_id;
} srtos_mutex_t;

/* --- MESSAGE QUEUE STRUCTURE --- */
typedef struct {
	uint32_t *buffer;
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t count;
	uint32_t capacity;
	volatile int waiting_receiver_id;
	volatile int waiting_sender_id;
} srtos_queue_t;

/* --- KERNEL CORE --- */
static inline __attribute__((always_inline)) uint32_t srtos_enter_critical(void) {
	uint32_t state;
	__asm__ __volatile__("rsil %0, 1" : "=r"(state)::"memory");
	return state;
}

static inline __attribute__((always_inline)) void srtos_exit_critical(uint32_t state) {
	__asm__ __volatile__("wsr %0, ps \n isync" ::"r"(state) : "memory");
}

static inline __attribute__((always_inline)) uint64_t srtos_get_time_us(void) {
	uint32_t ticks, ccount, ccomp;
	do {
		ticks = system_ticks;
		__asm__ __volatile__("rsr %0, ccount" : "=r"(ccount));
		__asm__ __volatile__("rsr %0, ccompare0" : "=r"(ccomp));
	} while (ticks != system_ticks);

	uint32_t last_tick_ccount = ccomp - SRTOS_CYCLES_PER_TICK;
	uint32_t cycles_passed = ccount - last_tick_ccount;
	return (uint64_t)ticks * (1000000ULL / CONFIG_TICK_RATE_HZ) + (cycles_passed / SRTOS_CYCLES_PER_US);
}

static inline __attribute__((always_inline)) void srtos_yield(void) {
	__asm__ __volatile__("wsr %0, intset \n isync" ::"r"(1 << 7));
}

static inline __attribute__((always_inline)) void srtos_delay_ms(uint32_t ms) {
	uint32_t state = srtos_enter_critical();
	task_list[current_task].wake_tick = system_ticks + ms;
	task_list[current_task].state = SRTOS_STATE_BLOCKED;
	srtos_exit_critical(state);
	srtos_yield();
}

void srtos_create_task(int task_id, int priority, void (*task_func)(), uint8_t *stack_mem);

/* Queue API */
static inline __attribute__((always_inline)) void srtos_queue_init(srtos_queue_t *q, uint32_t *buf,
																   uint32_t capacity) {
	q->buffer = buf;
	q->head = 0;
	q->tail = 0;
	q->count = 0;
	q->capacity = capacity;
	q->waiting_receiver_id = -1;
	q->waiting_sender_id = -1;
}

static inline __attribute__((always_inline)) void srtos_queue_send_blocking(srtos_queue_t *q, uint32_t item) {
	while (1) {
		uint32_t state = srtos_enter_critical();
		if (q->count < q->capacity) {
			q->buffer[q->head] = item;
			q->head = (q->head + 1) % q->capacity;
			q->count++;
			if (q->waiting_receiver_id != -1) {
				task_list[q->waiting_receiver_id].state = SRTOS_STATE_READY;
				q->waiting_receiver_id = -1;
			}
			srtos_exit_critical(state);
			return;
		}
		q->waiting_sender_id = current_task;
		task_list[current_task].state = SRTOS_STATE_WAIT_QUEUE;
		srtos_exit_critical(state);
		srtos_yield();
	}
}

static inline __attribute__((always_inline)) void srtos_queue_receive_blocking(srtos_queue_t *q,
																			   uint32_t *item) {
	while (1) {
		uint32_t state = srtos_enter_critical();
		if (q->count > 0) {
			*item = q->buffer[q->tail];
			q->tail = (q->tail + 1) % q->capacity;
			q->count--;
			if (q->waiting_sender_id != -1) {
				task_list[q->waiting_sender_id].state = SRTOS_STATE_READY;
				q->waiting_sender_id = -1;
			}
			srtos_exit_critical(state);
			return;
		}
		q->waiting_receiver_id = current_task;
		task_list[current_task].state = SRTOS_STATE_WAIT_QUEUE;
		srtos_exit_critical(state);
		srtos_yield();
	}
}

void srtos_mutex_init(srtos_mutex_t *m);
void srtos_mutex_take(srtos_mutex_t *m);
void srtos_mutex_give(srtos_mutex_t *m);

#endif /* OS_H */
