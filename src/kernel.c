#include "os.h"

typedef struct {
	uint32_t epc, eps, a[16], padding[2];
} ContextFrame;

volatile TCB_t task_list[SRTOS_MAX_TASKS];
volatile int current_task = 0;
volatile uint32_t system_ticks = 0;

void srtos_create_task(int task_id, int priority, void (*task_func)(), uint8_t *stack_mem) {
	uint32_t *top_of_stack = (uint32_t *)(stack_mem + SRTOS_STACK_SIZE);
	top_of_stack = (uint32_t *)((uint8_t *)top_of_stack - sizeof(ContextFrame));

	ContextFrame *frame = (ContextFrame *)top_of_stack;
	frame->epc = (uint32_t)task_func;

	frame->eps = 0x40010;

	for (int i = 0; i < 16; i++)
		frame->a[i] = 0;

	task_list[task_id].sp = top_of_stack;
	task_list[task_id].state = SRTOS_STATE_READY;
	task_list[task_id].base_priority = priority;
	task_list[task_id].priority = priority;
}

/* --- MUTEX IMPLEMENTATION --- */
void srtos_mutex_init(srtos_mutex_t *m) {
	m->locked = 0;
	m->owner_task_id = -1;
}

void srtos_mutex_take(srtos_mutex_t *m) {
	while (1) {
		uint32_t state = srtos_enter_critical();
		if (m->locked == 0) {
			m->locked = 1;
			m->owner_task_id = current_task;
			task_list[current_task].waiting_mutex = NULL; /* No more waiting */
			srtos_exit_critical(state);
			return;
		}

		/* Priority Inheritance */
		int owner = m->owner_task_id;
		if (owner != -1 && task_list[current_task].priority > task_list[owner].priority) {
			task_list[owner].priority = task_list[current_task].priority;
		}

		/* Sleep and save the mutex we're waiting for */
		task_list[current_task].waiting_mutex = (void *)m;
		task_list[current_task].state = SRTOS_STATE_WAIT_MUTEX;
		srtos_exit_critical(state);
		srtos_yield();
	}
}

void srtos_mutex_give(srtos_mutex_t *m) {
	uint32_t state = srtos_enter_critical();
	int higher_priority_woken = 0;

	if (m->owner_task_id == current_task) {
		/* Release the mutex */
		m->locked = 0;
		m->owner_task_id = -1;

		/* Wake up any tasks waiting for this mutex */
		for (int i = 0; i < SRTOS_MAX_TASKS; i++) {
			if (task_list[i].state == SRTOS_STATE_WAIT_MUTEX && task_list[i].waiting_mutex == (void *)m) {
				task_list[i].state = SRTOS_STATE_READY;
				task_list[i].waiting_mutex = NULL;

				/* If a higher priority task was woken up, we need to yield */
				if (task_list[i].priority > task_list[current_task].priority) {
					higher_priority_woken = 1;
				}
			}
		}

		/* (False Inheritance prevention inheritance calculation) */
		/* My priority should be at least my original priority */
		int max_inherited_priority = task_list[current_task].base_priority;

		for (int i = 0; i < SRTOS_MAX_TASKS; i++) {
			if (task_list[i].state == SRTOS_STATE_WAIT_MUTEX && task_list[i].waiting_mutex != NULL) {
				/* Get the information of the mutex that the task is waiting for */
				srtos_mutex_t *other_m = (srtos_mutex_t *)task_list[i].waiting_mutex;

				/* If the task is waiting for a mutex that I own */
				if (other_m->owner_task_id == current_task) {
					/* And its priority is higher than my current priority, update my priority */
					if (task_list[i].priority > max_inherited_priority) {
						max_inherited_priority = task_list[i].priority;
					}
				}
			}
		}

		/* Update my priority to the maximum inherited priority */
		task_list[current_task].priority = max_inherited_priority;
	}
	srtos_exit_critical(state);

	if (higher_priority_woken) {
		srtos_yield();
	}
}
