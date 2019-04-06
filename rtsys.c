/*
 *\brief	Implements SYSCALL Definitons, SYSFS Show and Store
 *		functions to fill up the cores with tasks
 *
 *\author	Shobhit Kukreti
 *\date
 *\file		rtsys.c
 */

#include <asm/neon.h>
#include "rtsys.h"

extern struct timespec diff_timespec(struct timespec tm1, struct timespec tm2);
struct workqueue_struct *exynos_wq;

void rtsched_work_handler(struct work_struct *work);

int rt_flag; EXPORT_SYMBOL(rt_flag);

struct work_struct sched_hi_work;

/* Work Queue to Schedule our Q Process */
DECLARE_WORK(sched_wq, rtsched_work_handler); EXPORT_SYMBOL(sched_wq);

/* Lock to Protect List */
DEFINE_MUTEX(mutex);
sched_queue *cpu0 = NULL, *cpu1 = NULL, *cpu2 = NULL, *cpu3 = NULL;
sched_queue *runqueue = NULL, *slpqueue = NULL, *crnttsk = NULL;

/*Check for prior pid entry, return the node for updating values */
sched_queue *check_dup(sched_queue **queue, pid_t pid)
{
	sched_queue *tmp = *queue;

	if (tmp == NULL)
		return NULL;
				while (tmp != NULL &&  tmp->pid	!= pid)
		tmp = tmp->next;

	return tmp;
}


/* Insert an Element in the Queue and return the element to fill parameters */

sched_queue *insert_queue(sched_queue **queue)
{
	sched_queue *tmp;

	if (!(*queue)) { /* Empty Queue */
		*queue = kzalloc(sizeof(sched_queue), GFP_ATOMIC);
		(*queue)->next = NULL;
		return *queue;
	}

	tmp = *queue;
	while (tmp->next != NULL)
		tmp = tmp->next;
	tmp = kzalloc(sizeof(sched_queue), GFP_ATOMIC);
	(*queue)->next =  tmp;
	tmp->next = NULL;
	return tmp;

}

sched_queue *create_node(void)
{
	sched_queue *node; node = kzalloc(sizeof(sched_queue), GFP_KERNEL);

	if (node == NULL)
		return NULL;
	node->next = NULL; node->cpuq = NULL;
	return node;
}


static ssize_t proc_time_show(struct kobject *kobj, struct attribute *attr, char
		*buf)
{
	struct proc_attr *sattr;
	sched_queue *node;

	sattr = container_of(attr, struct proc_attr, attr);
	node = container_of(sattr, sched_queue, file_attr);
	if (node)
		return scnprintf(buf, PAGE_SIZE, "P:%d, RC:%lld.%.9ld
			RT:%lld.%.9ld, C_ms:%d, T_ms:%d\n", node->pid, (long
			long)node->rawets.tv_sec, node->rawets.tv_nsec, (long
				long)node->rawpts.tv_sec, node->rawpts.tv_nsec,
			node->c_time, node->p_time);

	else
		return scnprintf(buf, PAGE_SIZE, "None\n");

}

/* (BAD IDEA) Setting Reservations from sysfs */
static ssize_t proc_time_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t len)
{
	int ret, pid, c, t; struct
	proc_attr * sattr; sched_queue *node;

	sattr = container_of(attr, struct proc_attr, attr);
	node =	container_of(sattr, sched_queue, file_attr);

	ret = sscanf(buf, "%d %d %d", &pid, &c, &t);
	/* Check for Error */
	pr_info("%s, %d, %d, %d", __func__, pid, c, t);

	node->c_time = c;
	node->p_time = t;

	return len;
}


static ssize_t proc_crnt_show(struct kobject *kobj, struct attribute *attr, char
		*buf)
{

	if (crnttsk)
		return scnprintf(buf, PAGE_SIZE, "P:%d E:%lld.%.9ld,C:
			%lld.%.9ld T %lld.%.9ld RC:%lld.%.9ld
			RT:%lld.%.9ld\n", crnttsk->pid, (long
			long)crnttsk->ets.tv_sec, crnttsk->ets.tv_nsec, (long
				long)crnttsk->cts.tv_sec, crnttsk->cts.tv_nsec,
			(long long)crnttsk->pts.tv_sec, crnttsk->pts.tv_nsec,
			(long
			 long)crnttsk->rawets.tv_sec, crnttsk->rawets.tv_nsec,
			(long
			 long)crnttsk->rawpts.tv_sec, crnttsk->rawpts.tv_nsec);

	else
		return scnprintf(buf, PAGE_SIZE, "Empty Q");

}


/* Kobject Attributes for Sysfs */

static struct sysfs_ops proc_ops = {
	.show = proc_time_show,
	.store = proc_time_store,

};


static struct sysfs_ops crnt_ops = { .show = proc_crnt_show, .store = NULL, };


static struct proc_attr crnt_info = { .attr.name = "current", .attr.mode = 0644,

};

static struct proc_attr dummy_info = { .attr.name = "dummy", .attr.mode = 0644,

};

static struct attribute *attr_direc[12] = { &crnt_info.attr, NULL };



static struct kobj_type direc_file = {
	.sysfs_ops = &crnt_ops,
	.default_attrs = attr_direc,
};

struct kobject *dkobj;

int init_sys(void)
{
	dkobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!dkobj)
		return -ENOMEM;
	kobject_init(dkobj, &direc_file);
	if (kobject_add(dkobj, NULL, "%s", "rtexynos")) {
		pr_err("%s, kobj_add failed\n", __func__);
		kobject_put(dkobj);
		kfree(dkobj);
	return -EFAULT;

	}
	/* init workq for creating sysfs for each pid */
	exynos_wq = alloc_workqueue("exynos_wq", WQ_HIGHPRI, WQ_DFL_ACTIVE);
	if (!exynos_wq) {
		pr_err("%s, exynos_wq -fault\n", __func__);
		return -ENOMEM;
	}

	INIT_WORK(&sched_hi_work, rtsched_work_handler);
	return 0;
}



void create_pid_sysfs(sched_queue *eq)
{

	rt_flag = 0;
	eq->file_attr.attr.name = "proc_info";
	eq->file_attr.attr.mode = 0644;
	eq->attr_grp[0] = &(eq->file_attr.attr);
	eq->attr_grp[1] = NULL;
	eq->sysfs_file.sysfs_ops = &proc_ops;
	eq->sysfs_file.default_attrs = eq->attr_grp;

	pr_err("%s, pid: %d\n", __func__, eq->pid); kobject_init(&eq->pid_kobj,
			&eq->sysfs_file);

	if (kobject_add(&eq->pid_kobj, dkobj, "%d", eq->pid)) {
		pr_err("%s, kobj_add failed\n", __func__);
			kobject_put(&eq->pid_kobj);
			rt_flag = 1;
			return;
	}
	rt_flag = 1;

}

/* Scheduler Hook */

void force_rt(pid_t next, pid_t prev, struct timespec *exec_value, int val)
{
	struct timespec rawts;
	sched_queue *tmp;

	if (!runqueue)
		return;

		tmp = check_dup(&runqueue, next);
		if (tmp != NULL) {

		if (tmp->rawts.tv_sec == 0 && tmp->rawts.tv_nsec == 0)
			getrawmonotonic(&tmp->rawts);
		else {
			getrawmonotonic(&rawts);
			tmp->rawpts = timespec_add(tmp->rawpts,
				timespec_sub(rawts, tmp->rawts));
				tmp->rawts.tv_sec = rawts.tv_sec;
				tmp->rawts.tv_nsec = rawts.tv_nsec;
		}

		if ((if_tmr_active(&tmp->tmr_c) == false) &&
				tmp->ctimer_state == CLK_STOP) {
			rsv_timer_start(&tmp->tmr_c, tmp->c_time - tmp->e_time
					);
			tmp->ctimer_state = CLK_RUN;
		}

		if (tmp->ptimer_state == CLK_STOP) {
			rsv_timer_start(&tmp->tmr_p,
				tmp->p_time); tmp->ptimer_state = CLK_RUN;
		}
		crnttsk = tmp;
	}

	tmp = check_dup(&runqueue, prev);
	if (tmp != NULL) {
		if (tmp->rawts.tv_sec == 0 && tmp->rawts.tv_nsec == 0) {
			tmp->rawpts = timespec_add(tmp->rawpts, *exec_value);
			getrawmonotonic(&(tmp->rawts));
		} else {
				getrawmonotonic(&rawts);
				tmp->rawpts = timespec_add(tmp->rawpts,
					timespec_sub(rawts, tmp->rawts));
				tmp->rawts.tv_sec = rawts.tv_sec;
				tmp->rawts.tv_nsec = rawts.tv_nsec;
		}
		tmp->rawets = timespec_add(tmp->rawets, *exec_value);

		if (if_tmr_active(&tmp->tmr_c) == true)	{
			tmp->e_time = rsv_timer_cancel(&tmp->tmr_c,
					*exec_value);
		}
		tmp->state = TSK_SUSP;
		tmp->ctimer_state = CLK_STOP;
	}
}

EXPORT_SYMBOL_GPL(force_rt);

inline void sys_cleanup(sched_queue *node)
{
	kobject_put(&node->pid_kobj);
}


/* Remove item from the list */
void rt_exit(pid_t pid)
{

	sched_queue *tmp;
	/*set flag to dequeue the struct from our list */

	tmp = check_dup(&runqueue, pid);
	if (tmp != NULL) {
		sys_cleanup(tmp);
		tmp->raised_exit = 1;
		queue_work(exynos_wq, &sched_hi_work);
		pr_info("%s, Exit->%d\n", __func__, tmp->pid);
	}
}


EXPORT_SYMBOL(rt_exit);

// set protect==1 to prevent work_queue from accessing the
// the linked list
atomic_t protect;

/* Sched Work Handler */
void rtsched_work_handler(struct work_struct *work)
{
	sched_queue *node = runqueue;
	struct task_struct *tsk;

/* Protect with Mutex as Queue Structure may change if another syscall adds task
 */

	if (node == NULL) {
		pr_err("RQE\n");
		return;
	}

	if (atomic_read(&protect)==1) 
		return;

	while (node != NULL) {
		if (node->raised_exit == 1)
			delete_node(node->pid);
		node = node->next;
	}
	schedule();

}

/* New System Call */
asmlinkage long sys_rtexynos(pid_t procid, int comp_time, int period)
{
	sched_queue *tmp;
	struct task_struct *tsk;
	int ret = 0;

	pr_info("%s +\n", __func__);
	tmp = check_dup(&runqueue, (pid_t)procid);
	if (tmp != NULL) {
		spin_lock(&(tmp->lock));
		tmp->c_time = comp_time;
		tmp->cts.tv_nsec = MS_TO_NS(comp_time);
		tmp->p_time = period;
		tmp->pts.tv_nsec = MS_TO_NS(period);

		tmp->tmr_c.ts.tv_sec = 0;
		tmp->tmr_c.ts.tv_nsec = MS_TO_NS(comp_time);
		tmp->tmr_p.ts.tv_sec = 0;
		tmp->tmr_p.ts.tv_nsec = MS_TO_NS(period);
		if (!crnttsk)
			crnttsk = tmp;
		spin_unlock(&(tmp->lock));
	} else {
		if (is_os_core_timer_active() == true)
			 os_core_timer_cancel();

		tmp = create_node();
		if (tmp == NULL) {
			pr_err("%s, ENOMEM Node\n", __func__); return 0;
		}
		spin_lock_init(&(tmp->lock));

		ret = spin_trylock(&(tmp->lock));
		if (!ret) {
			pr_err("%s, Failed to get spinlock\n", __func__);
			return -1;
		}
		pr_info("insert q: %d, %d, %d\n", procid, comp_time, period);

		tmp->pid = procid;
		tmp->c_time = comp_time;
		tmp->p_time = period;
		tmp->e_time = 0;

		tmp->tmr_c.active = false;
		tmp->tmr_c.fired = false;
		tmp->tmr_c.timer_callback = &c_callback;
		tmp->tmr_c.rawtime.tv_sec = 0; tmp->tmr_c.rawtime.tv_nsec = 0;
		tmp->tmr_c.ts.tv_sec = 0;
		tmp->tmr_c.ts.tv_nsec = MS_TO_NS(comp_time);

		tmp->tmr_p.active = false;
		tmp->tmr_p.fired = false;
		tmp->tmr_p.timer_callback = &p_callback;
		tmp->tmr_p.rawtime.tv_nsec = 0;
		tmp->tmr_p.rawtime.tv_nsec = 0;
		tmp->tmr_p.ts.tv_sec = 0;
		tmp->tmr_p.ts.tv_nsec = MS_TO_NS(period);


		tmp->rawts.tv_sec = 0;
		tmp->rawts.tv_nsec = 0;
		tmp->rawets.tv_sec = 0;
		tmp->rawets.tv_nsec = 0;
		tmp->state = TSK_RUN;
		tmp->raised_exit = -1;

		tmp->ctimer_state = CLK_STOP;
		tmp->ptimer_state = CLK_STOP;

		tsk = find_task_by_vpid(procid);
		if (tsk == NULL) {
			spin_unlock(&(tmp->lock));
			return -EFAULT;
		}
		tmp->utils = comp_time*1000/period;

		pr_info("PR:%d, STPR:%d, NMPR:%d, RTPR:%u\n", tsk->prio,
				tsk->static_prio, tsk->normal_prio,
				tsk->rt_priority);
		tsk->prio = tsk->static_prio = tsk->normal_prio = 105;
		tsk->policy = SCHED_RR;	spin_unlock(&(tmp->lock));

		/* Insert Node in Per processor Queue */

		atomic_set(&protect,1);
		insert_q_descending(&tmp);
		if (wfd_packing() == -1)
			delete_node(procid);

		create_pid_sysfs(tmp);
		if (assign_task_core(cpu0, 4) != -1)
			print_list(cpu0, 4);

		if (assign_task_core(cpu1, 5) != -1)
			print_list(cpu1, 5);
		if (assign_task_core(cpu2, 6) != -1)
			print_list(cpu2, 6);
		if (assign_task_core(cpu3, 7) != -1)
			print_list(cpu3, 7);

		get_process_affinity(tmp->pid);
		cpu0 = NULL;
		cpu1 = NULL;
		cpu2 = NULL;
		cpu3 = NULL;
		atomic_set(&protect,0);
		os_core_timer_init();
	}

	rt_flag = 1;
	pr_info("%s\n", __func__);
	return 0;
}


/* Create directories on startup */
late_initcall(init_sys);




