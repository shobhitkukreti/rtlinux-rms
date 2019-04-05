/*
 *\brief	Abstracted Timer API's which produce a notion of TICK
 *		for our scheduler based on HR Timers
 *\author	Shobhit Kukreti
 *\date
 *\file		timekeeping.c
 * \issue	fix magic numbers
 */

#include "rtsys.h"
#include <linux/time.h>

extern struct work_struct sched_wq, sched_hi_work;
extern struct workqueue_struct *exynos_wq;
extern int protect;
extern sched_queue *runqueue;

static int first_entry;

struct timespec diff_timespec(struct timespec tm1, struct timespec tm2)
{
	struct	timespec result;
 if ((tm1.tv_sec < tm2.tv_sec) || ((tm1.tv_sec == tm2.tv_sec) && (tm1.tv_nsec <=
				 tm2.tv_nsec))){ /* TIME1 <= TIME2? */
	 result.tv_sec = result.tv_nsec = 0;
	}	else{
		/* TIME1 > TIME2 */
		result.tv_sec = tm1.tv_sec - tm2.tv_sec ;
		 if (tm1.tv_nsec < tm2.tv_nsec) {
			result.tv_nsec = tm1.tv_nsec +  1000000000L - tm2.tv_nsec;
			result.tv_sec--;
			/*Borrow a second.*/
			}	else{
				result.tv_nsec = tm1.tv_nsec - tm2.tv_nsec;
			}
	}
	return result;
}

static struct timespec prev_ts;

void os_tick_update(void)
{
	unsigned long long nsec, msec;
	struct timespec diffts, ts, sub;
	long int diffns = 0;
	sched_queue *node = runqueue;

	diffts.tv_sec = 0;
	diffts.tv_nsec = 0;

	if (!first_entry) {
		getrawmonotonic(&ts);
		prev_ts.tv_sec = ts.tv_sec;
		prev_ts.tv_nsec = ts.tv_nsec;
		first_entry = 1;
		return;
	}	else
			getrawmonotonic(&ts);

	diffts = diff_timespec(ts, prev_ts);
	prev_ts.tv_sec = ts.tv_sec;
	prev_ts.tv_nsec = ts.tv_nsec;
	sub.tv_sec = 0;
	sub.tv_nsec = 0;
	while (node != NULL) {
		if (node->tmr_c.active == true) {
			node->tmr_c.rawtime.tv_nsec += diffts.tv_nsec;
			diffns = node->tmr_c.ts.tv_nsec -
				node->tmr_c.rawtime.tv_nsec;
			diffns = diffns/1000;
			if (diffns <= 999) {
				node->tmr_c.fired = true;
				node->tmr_c.rawtime.tv_sec = 0;
				node->tmr_c.rawtime.tv_nsec = 0;

			}

		}
		if (node->tmr_p.active == true) {
			node->tmr_p.rawtime.tv_nsec +=
			diffts.tv_nsec;
			diffns = node->tmr_p.ts.tv_nsec -
				node->tmr_p.rawtime.tv_nsec;
			diffns = diffns/1000;
			if (diffns < 0) {
				node->tmr_p.fired = true;
				node->tmr_p.rawtime.tv_sec = 0;
				node->tmr_p.rawtime.tv_nsec = 0;
			}
		}
		node = node->next;
	}
}

void os_callback(void)
{
	sched_queue *node = runqueue;

	while (node != NULL) {
	if (node->tmr_c.fired == true) {
		node->tmr_c.timer_callback(node);
		node->tmr_c.fired = false;
		node->tmr_c.active = false;
		}
	if (node->tmr_p.fired == true) {
		node->tmr_p.timer_callback(node);
		node->tmr_p.fired = false;
		}
		node = node->next;
	}
}


/*Restart*/
int p_callback (sched_queue *node)
{
	struct task_struct *tsk;

	node->state = TSK_RUN;
	node->ets.tv_sec = 0;
	node->ets.tv_nsec = 0;
	node->e_time = 0;
	tsk = find_task_by_vpid(node->pid);
	wake_up_process(tsk);
	queue_work(exynos_wq, &sched_hi_work);
	return 1;
}


/* No Restart */
int c_callback(sched_queue *node)
{
	struct task_struct *tsk;

	node->ctimer_state = CLK_STOP;
	node->state = TSK_SUSP;
	tsk = find_task_by_vpid(node->pid);
	set_tsk_need_resched(tsk);
	set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	queue_work(exynos_wq, &sched_hi_work);
	return 0;

}


bool if_tmr_active(rsv_t *tmr)
{
	return tmr->active;
}


int rsv_timer_start(rsv_t *tmr, int msleep)
{
	tmr->ts.tv_nsec = MS_TO_NS(msleep);
	tmr->active = true;
	return 0;

}


unsigned long long rsv_timer_cancel(rsv_t *tmr, struct timespec exec)
{
	long long nsec, sumns; struct timespec diffts, ts;

	tmr->active = false;
	nsec = tmr->ts.tv_nsec - exec.tv_nsec;
	nsec = nsec/1000;
	sumns = tmr->rawtime.tv_nsec + exec.tv_nsec;
	if ((tmr->ts.tv_nsec - sumns) < 400)
		tmr->fired = true;
	return	(nsec/1000);

}


struct hrtimer core_tick;

static enum hrtimer_restart hrtimer_thread_callback(struct hrtimer *timer)
{
	ktime_t kt_now;
	unsigned long ovrrun;

	os_tick_update();
	if (!protect)
		os_callback();
	for (;;) {
		kt_now = hrtimer_cb_get_time(timer);
		ovrrun = hrtimer_forward(timer,
			kt_now, ktime_set(0, SCHED_TIME_NS));
		if (!ovrrun)
			break;
	}
	return	HRTIMER_RESTART;
}



int os_core_timer_init (void)
{
	pr_err("%s\n", __func__);
	hrtimer_init(&core_tick, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	core_tick.function = &hrtimer_thread_callback;
	hrtimer_start(&core_tick, ktime_set(0, SCHED_TIME_NS),
					HRTIMER_MODE_REL_PINNED);
	return	0;
}


int os_core_timer_cancel(void)
{
	pr_err("Disable RTEXYNOS Premept\n");
	hrtimer_cancel(&core_tick);
	return 0;
}

bool is_os_core_timer_active(void)
{
	return hrtimer_active(&core_tick);
}
