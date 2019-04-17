/*
 A Thread Library Implementation 

 Authors: Julian Bertini & Mitchel Smith
*/

#include <stddef.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include <sys/time.h>

#include "threads.h"

int created = 0, createdThreadNumber, currentActiveThread = 0;
tcb_t threadContextArray[MAX_THREADS];
struct itimerval timeInfo, timeDisabled;
struct timeval timeValue = {
    .tv_sec = 0,
    .tv_usec = TIMER_VAL
};
struct timeval timeInterval = {
    .tv_sec = 0,
    .tv_usec = TIMER_VAL
};
struct timeval zeroTime = {
    .tv_sec = 0,
    .tv_usec = 0
};

// This sets up all the signals that our program will interpret.
void setupSignalHandler(int signal, void (*handler)(int)) {
	struct sigaction options;

	memset(&options, 0, sizeof(struct sigaction));
	options.sa_handler = handler;

	if(sigaction(signal, &options, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

void sigusr_handler(int signal_number) {
	if(setjmp(threadContextArray[createdThreadNumber].buffer) == 0) {
		created = 1;
	}
	else { // here we invoke callback function for each thread after it has been created
        setitimer(ITIMER_REAL,&timeInfo,NULL); // reset timer for current thread
        void *argument = threadContextArray[currentActiveThread].argument;
		threadContextArray[currentActiveThread].function(argument);
	}
}

void sigAlarmHandler(int signal) {
    thread_yield();
}

void thread_init(int set_preemption){
    struct sigaction sigusr_hints;

    // initialize the signal handler for thread creation
	memset(&sigusr_hints, 0, sizeof(struct sigaction));
	sigusr_hints.sa_handler = sigusr_handler;
	sigusr_hints.sa_flags = SA_ONSTACK; 
	sigemptyset(&sigusr_hints.sa_mask);

	if(sigaction(SIGUSR1, &sigusr_hints, NULL) == -1) {
		perror("sigaction/SIGUSR1");
		exit(EXIT_FAILURE);
	}

    setupSignalHandler(SIGALRM, sigAlarmHandler);
 
    for(int i = 1; i < MAX_THREADS; i++) {
        threadContextArray[i].number = i;
        threadContextArray[i].state = STATE_INVALID;
        threadContextArray[i].function = NULL;
        threadContextArray[i].argument = NULL;
        threadContextArray[i].return_value = NULL;
        threadContextArray[i].joiner_thread_number = NO_JOINER;
        threadContextArray[i].stack = NULL;
    }
    threadContextArray[0].number = 0;
    threadContextArray[0].state = STATE_ACTIVE;
    threadContextArray[0].function = NULL;
    threadContextArray[0].argument = NULL;
    threadContextArray[0].return_value = NULL;
    threadContextArray[0].joiner_thread_number = NO_JOINER;
    threadContextArray[0].stack = NULL;

    timeInfo.it_interval = timeInterval;
    timeInfo.it_value = timeValue;
    timeDisabled.it_interval = timeInterval;
    timeDisabled.it_value = zeroTime;

    // setitimer(ITIMER_REAL,&timeInfo,NULL);
}

int thread_create(void *(*function)(void *), void *argument) {
    stack_t new_stack;
	stack_t old_stack;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threadContextArray[i].state == STATE_INVALID) {
            threadContextArray[i].state = STATE_ACTIVE;
            threadContextArray[i].argument = argument;
            threadContextArray[i].function = function;
            threadContextArray[i].stack = (char *)malloc(STACK_SIZE);

            // We have to indicate that the signal should be processed in
            // the stack of each current active thread.  
            new_stack.ss_flags = 0;
            new_stack.ss_size = STACK_SIZE;
            new_stack.ss_sp = threadContextArray[i].stack;

            if(sigaltstack(&new_stack, &old_stack) == -1) {
                perror("sigaltstack");
                exit(EXIT_FAILURE);
            }

            // need this since currentActiveThread != new created thread ID
            // used in signal handler
            createdThreadNumber = i; 

            raise(SIGUSR1);

	        while(!created) {}; // wait for the thread setjmp buffer to be initialized 
            created = 0; // need to reset created after each new thread

            return threadContextArray[i].number;
        }
    }
    return 0;
}
int thread_yield(){

    setitimer(ITIMER_REAL,&timeDisabled,NULL); // disable timer

	for(int i = currentActiveThread + 1; i != currentActiveThread % MAX_THREADS; i++){
        if(threadContextArray[i % MAX_THREADS].state == STATE_ACTIVE) {
            if (setjmp(threadContextArray[currentActiveThread].buffer) == 0) {

                currentActiveThread = threadContextArray[i % MAX_THREADS].number;

                longjmp(threadContextArray[currentActiveThread].buffer, 1);

            } else {
                setitimer(ITIMER_REAL,&timeInfo,NULL); // reset timer
                break; // once we switch back to this thread, we move on
            }
        }
    }
    return 0;
}
void thread_exit(void *return_value) {
    threadContextArray[currentActiveThread].state = STATE_FINISHED;
    threadContextArray[currentActiveThread].return_value = return_value;

    free(threadContextArray[currentActiveThread].stack);

    // no need to setjmp here since this thread is done and we're never coming back.
    if (threadContextArray[currentActiveThread].joiner_thread_number != -1) {
        currentActiveThread = threadContextArray[currentActiveThread].joiner_thread_number;
        longjmp(threadContextArray[currentActiveThread].buffer, 1);
    } else {
        thread_yield();
    }
}

void thread_join(int target_thread_number){
    // a thread that calls join will not be reactivated until the thread it's
    // waiting on calls thread_exit
    threadContextArray[currentActiveThread].state = STATE_BLOCKED;
    threadContextArray[target_thread_number].joiner_thread_number = currentActiveThread;

    if (setjmp(threadContextArray[currentActiveThread].buffer) == 0) {
        currentActiveThread = target_thread_number;
        longjmp(threadContextArray[currentActiveThread].buffer,1);
    } else {
        setitimer(ITIMER_REAL,&timeInfo,NULL); // reset timer
        threadContextArray[currentActiveThread].state = STATE_ACTIVE;
    }
}