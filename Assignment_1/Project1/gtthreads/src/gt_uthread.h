#ifndef __GT_UTHREAD_H
#define __GT_UTHREAD_H

/* User-level thread implementation (using alternate signal stacks) */

typedef unsigned int uthread_t;
typedef unsigned int uthread_group_t;

/* uthread states */
#define UTHREAD_INIT 0x01
#define UTHREAD_RUNNABLE 0x02
#define UTHREAD_RUNNING 0x04
#define UTHREAD_CANCELLED 0x08
#define UTHREAD_DONE 0x10

/* uthread struct : has all the uthread context info */
typedef struct uthread_struct
{

	int uthread_state; /* UTHREAD_INIT, UTHREAD_RUNNABLE, UTHREAD_RUNNING, UTHREAD_CANCELLED, UTHREAD_DONE */
	int uthread_priority; /* uthread running priority */
	int cpu_id; /* cpu it is currently executing on */
	int last_cpu_id; /* last cpu it was executing on */

	uthread_t uthread_tid; /* thread id */
	uthread_group_t uthread_gid; /* thread group id  */
	int (*uthread_func)(void*);
	void *uthread_arg;
	
	int initial_credits; /* initial credits for the thread */
	int credits; /* credits for the thread */

	struct timeval run_time_total; /* total running time */
	struct timeval run_time_step; /* running time in a step */
	struct timeval start_running_time_step; /* start running time for a step*/
	struct timeval end_running_time_step; /* end running time for a step */

	struct timeval total_running_time_start; /* start total running time */
	struct timeval total_running_time_end; /* end total running time */


	void *exit_status; /* exit status */
	int reserved1;
	int reserved2;
	int reserved3;

	int is_yield_valid;

	sigjmp_buf uthread_env; /* 156 bytes : save user-level thread context*/
	stack_t uthread_stack; /* 12 bytes : user-level thread stack */
	TAILQ_ENTRY(uthread_struct) uthread_runq;
} uthread_struct_t;

typedef struct _uthread_time
{
	struct timeval running_time;
	struct timeval cpu_time;
} uthread_timer_t;

struct __kthread_runqueue;
extern void uthread_schedule(uthread_struct_t * (*kthread_best_sched_uthread)(struct __kthread_runqueue *));
extern void gt_yield();
extern uthread_timer_t *uthread_time[];
#endif
