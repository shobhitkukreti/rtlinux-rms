/*
 * \brief	Header File containing macros and structs for implementing Real
 *		Time Linux (Rate Monotonic Scheduling)
 *
 * \author	Shobhit Kukreti
 * \date
 * \file	rtsys.h
 * \issue
 */

#ifndef RTSYS_H_
#define RTSYS_H_

#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/kobject.h>

/* Macros for state of queue */
#define TSK_RUN 97
#define TSK_SUSP 98 // Task_UNINTERRUPTIBLE
#define TSK_STP  99
#define CLK_RUN 100
#define CLK_STOP 101

#define MS_TO_NS(x) (x * 1000000UL)

#define SCHED_TIME_NS 1000000UL
#define SCHED_SCALE(x) (x/SCHED_TIME)

struct proc_attr {
	struct attribute attr;
};


struct exynos_queue;
typedef struct exynos_queue sched_queue;

struct wq_sysfs {
	struct work_struct work;
	sched_queue *node;
};


struct reservation_timer{
	bool active;
	bool fired;
	struct	timespec rawtime, ts;
	int (*timer_callback)(sched_queue *);

};

typedef struct reservation_timer rsv_t;


struct exynos_queue{
	struct task_struct *tsk;
	struct exynos_queue *next, *prev, *cpuq;
	int utils, cpu; /* Total is 1000 */
	struct timespec cts, pts, ets, rawts, rawets, rawpts;
	spinlock_t lock;
	int state, ctimer_state, ptimer_state, raised_exit;
	int e_time, c_time, p_time;
	pid_t pid;
	struct kobject pid_kobj;
	struct proc_attr file_attr;
	struct attribute *attr_grp[2];
	struct kobj_type sysfs_file;
	rsv_t tmr_c, tmr_p;
	struct wq_sysfs sysfs_wq;
};



int insert_q_descending(sched_queue **node);
int assign_cpu(void);
int delete_node(pid_t pid);
void print_list(sched_queue *list, int val);
int get_process_affinity(pid_t pid);
int wfd_packing(void);
int set_process_affinity(int cpu, pid_t pid);
int assign_task_core(sched_queue *cpu, int core);

void os_tick_update(void);
void os_callback(void);
int c_callback(sched_queue *);
int p_callback(sched_queue *);
int rsv_timer_start(rsv_t *, int msleep);
unsigned long long rsv_timer_cancel(rsv_t *, struct timespec);
int os_core_timer_init(void);
int os_core_timer_cancel(void);
bool if_tmr_active(rsv_t *tmr);
bool is_os_core_timer_active(void);


#endif
