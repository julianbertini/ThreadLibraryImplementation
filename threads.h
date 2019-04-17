/*
 * Copyright (c) 2017, Hammurabi Mendes.
 * License: BSD 2-clause
 */
#ifndef THREADS_H
#define THREADS_H

#define STATE_ACTIVE   0
#define STATE_BLOCKED  1
#define STATE_FINISHED 2
#define STATE_INVALID  3
#define NO_JOINER -1
#define STACK_SIZE 64000
#define TIMER_VAL 1000

#define MAX_THREADS 128

#include <setjmp.h>

struct tcb_t_ {
	int number;
	jmp_buf buffer;
	char *stack;

	void *(*function)(void *);
	void *argument;
	void *return_value;

	int state;
	int joiner_thread_number;
};

typedef struct tcb_t_ tcb_t;

extern tcb_t threadContextArray[MAX_THREADS];
extern int currentActiveThread;

void thread_init(int preemption_enabled);
int thread_create(void *(*function)(void *), void *argument);
int thread_yield(void);
void thread_join(int target_thread_number);
void thread_exit(void *return_value);

#endif /* THREADS_H */
