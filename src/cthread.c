#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/support.h"
#include "../include/cthread.h"
#include "../include/cdata.h"
#include "../include/priority_queue.h"

/* Configure default values */
#define FIRST_TID 0
#define STACK_SIZE (2 << 20) /* Each stack has 1MB of memory */

#define LOG_ENABLED /* Comment/Uncomment to Disable/Enable log file */
#ifdef LOG_ENABLED
#define DEBUG(s_, ...) fprintf(cthread_file, "[DEBUG] " s_ "\n", ##__VA_ARGS__)
#define ERROR(s_, ...) fprintf(cthread_file, "[ERROR] " s_ "\n", ##__VA_ARGS__)
#define CREATE_LOG_FILE (cthread_file = fopen("./cthread.log", "a"))
#else
#define DEBUG(s_, ...)
#define ERROR(s_, ...)
#define CREATE_LOG_FILE
#endif

/* Configure global variables */
TCB_t *running = NULL;
PriorityQueue *ready = NULL;
PriorityQueue *blocked = NULL;
ucontext_t *scheduler_context = NULL;
int global_tid = FIRST_TID;

/* Configure DEBUG file */
FILE *cthread_file;

/* Prototypes to auxiliar functions */
int initialize_cthread();
void scheduler();
void wakeUp(int);

int ccreate(void *(*start)(void *), void *arg, int prio)
{
	TCB_t *new_thread = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	/* Allocate new thread TCB */
	new_thread = (TCB_t *)malloc(sizeof(TCB_t));
	new_thread->state = PROCST_APTO;
	new_thread->prio = prio;
	new_thread->tid = global_tid++;
	new_thread->waited_by = NULL;
	DEBUG("Created new thread with TID %d", new_thread->tid);

	/* Initialize context of it */
	DEBUG("Trying to acquire context for new thread");
	if (getcontext(&new_thread->context) == -1)
	{
		return -1;
		ERROR("Couldn't get thread context...");
	}

	DEBUG("Acquired thread context successfuly");
	new_thread->context.uc_link = scheduler_context;
	new_thread->context.uc_stack.ss_sp = malloc(STACK_SIZE);
	new_thread->context.uc_stack.ss_size = STACK_SIZE;
	new_thread->context.uc_stack.ss_flags = 0;
	makecontext(&new_thread->context, (void (*)(void))start, 1, arg);

	/* Add it to the Ready Queue */
	DEBUG("Inserting thread to ready Queue");
	insertPriorityQueue(ready, (void *)new_thread);

	DEBUG("Finished creating thread successfully");
	return new_thread->tid;
}

int cyield(void)
{
	TCB_t *current_thread = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	/* Update current_thread status to ready */
	current_thread = running;
	current_thread->state = PROCST_APTO;
	current_thread->prio = stopTimer(); /* Time since started running */

	DEBUG("Moving current running thread with tid %d to ready", current_thread->tid);
	insertPriorityQueue(ready, current_thread);

	/* Remove current running thread */
	running = NULL;

	/* Swap context to the scheduler decide who runs */
	DEBUG("Swapping context to scheduler");
	if (swapcontext(&(current_thread->context), scheduler_context) == -1)
	{
		ERROR("Couldn't swap context");
		return -1;
	}

	return 0;
}

int cjoin(int tid)
{
	TCB_t *joined_thread = NULL, *current_thread = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	/* Find the thread we want to join */
	DEBUG("Finding thread to join");
	current_thread = running;
	joined_thread = (TCB_t *)findPriorityQueue(ready, tid);

	/* If it is trying to wait for main, we return an error */
	if (tid == FIRST_TID)
	{
		ERROR("Tried to wait for main thread, which is NOT allowed");
		return -1;
	}

	/* If this thread doesn't exist, we return an error */
	if (joined_thread == NULL)
	{
		ERROR("Tried to wait for non existant thread");
		return -1;
	}

	/* If there is already somebody waiting for this, we return an error */
	if (joined_thread->waited_by != NULL)
	{
		ERROR("Tried to wait for a thread which is already waited by thread tid %d", joined_thread->waited_by->tid);
		return -1;
	}

	/* Update waited_thread to mark current_thread as waiting for it */
	DEBUG("Thread tid %d is waited by thread tid %d", tid, current_thread->tid);
	joined_thread->waited_by = current_thread;

	/* Update current_thread status to blocked, and add it to their list */
	DEBUG("Blocking current thread, adding it to the blocked queue");
	current_thread->state = PROCST_BLOQ;
	current_thread->prio = stopTimer();
	insertPriorityQueue(blocked, current_thread);

	/* Remove current running thread */
	running = NULL;

	/* Swap context to the scheduler decide who runs */
	DEBUG("Swapping context to scheduler");
	if (swapcontext(&(current_thread->context), scheduler_context) == -1)
	{
		ERROR("Couldn't swap context");
		return -1;
	}

	return 0;
}

int csem_init(csem_t *sem, int count)
{
	PriorityQueue *semaphore_queue = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	DEBUG("Create semaphore_queue");
	semaphore_queue = createPriorityQueue();

	DEBUG("Starting sem_count and sem_fila");
	sem->count = count;
	sem->fila = semaphore_queue;

	return 0;
}

int cwait(csem_t *sem)
{
	TCB_t *current_thread = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	DEBUG("Fetching running thread");
	current_thread = running;

	DEBUG("Starting cwait | P(s)");
	sem->count--;
	if (sem->count < 0)
	{

		DEBUG("Sleeping current_thread");
		current_thread->state = PROCST_BLOQ;
		current_thread->prio = stopTimer();

		DEBUG("Inserting thread in semaphoreQueue");
		insertPriorityQueue(sem->fila, current_thread);
		DEBUG("Thread inserted in queue");

		running = NULL;

		/* Swap context to the scheduler decide who runs */
		DEBUG("Swapping context to scheduler");
		if (swapcontext(&(current_thread->context), scheduler_context) == -1)
		{
			ERROR("Couldn't swap context");
			return -1;
		}
	}

	return 0;
}

int csignal(csem_t *sem)
{
	TCB_t *next_thread = NULL;

	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	DEBUG("Starting csignal | V(s)");
	sem->count++;
	if (sem->count <= 0)
	{

		DEBUG("Getting next thread from semaphore_queue");
		next_thread = frontPriorityQueue(sem->fila);

		if (next_thread == NULL)
		{
			ERROR("No thread in semaphore_queue");
		}
		else
		{
			DEBUG("Popped from semaphore_queue");
			popPriorityQueue(sem->fila);

			DEBUG("Changed state of next thread");
			next_thread->state = PROCST_APTO;
			insertPriorityQueue(ready, (void *)next_thread);

			DEBUG("Finished csignal");
		}
	}

	return 0;
}

int cidentify(char *name, int size)
{
	/* Initialize cthread library if not initialized yet */
	if (running == NULL && initialize_cthread() != 0)
	{
		ERROR("Couldn't initialize cthread");
		return -1;
	}

	strncpy(name, "Ana Carolina Pagnoncelli - 00287714\nAugusto Zanella Bardini  - 00278083\nRafael Baldasso Audibert - 00287695", size);
	return 0;
}

/* AUXILIAR FUNCTIONS */

/* First time running a cthread function, so need to create TCB for
   main and PriorityQueues for ready and blocked */
int initialize_cthread()
{
	/* Configure logs */
	CREATE_LOG_FILE;

	/* Create main function TCB_t and configure it */
	TCB_t *main_TCB = (TCB_t *)malloc(sizeof(TCB_t));
	main_TCB->state = PROCST_EXEC;
	main_TCB->prio = 0;
	main_TCB->tid = global_tid++;
	main_TCB->waited_by = NULL;
	DEBUG("Created main thread with TID %d", main_TCB->tid);

	/* Configure the global variables */
	running = main_TCB;
	ready = createPriorityQueue();
	blocked = createPriorityQueue();
	DEBUG("Configured global variables");

	/* Configure scheduler_context (with utc_link pointing to itself - infinite loop) */
	scheduler_context = (ucontext_t *)malloc(sizeof(ucontext_t));
	if (getcontext(scheduler_context) == 0)
	{
		scheduler_context->uc_link = scheduler_context;
		scheduler_context->uc_stack.ss_sp = malloc(STACK_SIZE);
		scheduler_context->uc_stack.ss_size = STACK_SIZE;
		scheduler_context->uc_stack.ss_flags = 0;
		makecontext(scheduler_context, scheduler, 1, NULL);
		DEBUG("Successfully configured scheduler context");
	}
	else
	{
		ERROR("Couldn't get context for main thread");
		return -1;
	};

	/* Start counting the time for the main thread */
	startTimer();

	return 0;
}

/* Scheduler */
void scheduler()
{
	TCB_t *scheduled;

	DEBUG("Running scheduler");

	/* If running is not null, this means the thread it was there just finished */
	if (running != NULL)
	{
		DEBUG("Thread tid %d just finished", running->tid);
		if (running->waited_by != NULL)
		{
			DEBUG("Waking up thread tid %d children with tid %d", running->tid, running->waited_by->tid);
			wakeUp(running->waited_by->tid);
		}

		/* Free memory, as it already finished */
		free(running);

		/* Clean the space for running, to schedule new value */
		running = NULL;
	}

	scheduled = frontPriorityQueue(ready);
	if (scheduled == NULL)
	{
		ERROR("There is no thread to be scheduled");
		ERROR("EXITING with status code 1");
		exit(1);
	}

	/* Remove the entry from the priority queue */
	popPriorityQueue(ready);

	/* Update state */
	scheduled->state = PROCST_EXEC;

	/* Update running */
	running = scheduled;

	/* Start running priority timer */
	DEBUG("Starting timer");
	startTimer();

	/* Change context to the scheduled thread */
	DEBUG("Starting to run scheduled thread tid %d", running->tid);

	if (setcontext(&(scheduled->context)) == -1)
	{
		ERROR("Couldn't set context");
		ERROR("Exiting with error code 1");
		exit(1);
	}
}

/* Remove the tid with this value from the blocked list, and add it to the ready one */
void wakeUp(int tid)
{
	TCB_t *unblocked;

	DEBUG("Trying to wake up thread %d", tid);
	unblocked = (TCB_t *)findPriorityQueue(blocked, tid);

	/* If it was a valid thread to be unblocked, we add it to the ready ones */
	if (unblocked != NULL)
	{
		DEBUG("Waking up thread %d, removing it from blocked and adding to ready", tid);
		removePriorityQueue(blocked, tid);
		insertPriorityQueue(ready, (void *)unblocked);
	}
	else
	{
		ERROR("Thread %d not found", tid);
	}

	return;
}
