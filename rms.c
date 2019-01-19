/*!
 * \brief 	    Calculation for Worst Fit Decreasing Bin Packing
 *				to fill up the cores with tasks
 *		        
 * \author      Shobhit Kukreti
 * \date       
 * \file        rms.c
 *
 */



#include "rtsys.h"

/* Load a Processor Core to 69% only leaving the rest for back ground tasks 
 * This simple bin packing scheme ensures tasks are always schedulable
 * Do not add a task if it is not schedulable
 * */

//numbers for simplest rms scheduling without RT Test
static int rms[11] = {1000, 828, 780, 757, 743, 735, 729, 724, 721, 718, 693};

extern sched_queue *cpu0,*cpu1,*cpu2,*cpu3;
extern sched_queue *runqueue, *slpqueue,*crnttsk; 

void print_list(sched_queue *list, int val)
{
	
	char str[512];
	int ret =0;

	if(list==NULL)
		return;

	ret = sprintf(str, "CPU[%d]:",val);
	while(list!=NULL) {
		ret += sprintf(str+ret, "Util[%d], P:[%d]\n",list->utils, list->p_time);
		if(val!=0)
			list=list->cpuq; 
		else
			list = list->next;
	}
	if(val!=0)
		pr_info("%s\n",str);
	else
		pr_info("RUNQUEUE: %s\n", str);
	
}


/* Check for schedulabitility */
int is_schedulable(sched_queue **list)
{
        int rtp =0, rtn=0; /* Response Time for Prev and Next RT */
        int cel=0;
        sched_queue *t1 = *list;
        sched_queue *t2;

        if(t1==NULL)
                return 1; /*Empty List, Always Schedulable */

        if(t1->cpuq==NULL)
                if(t1->c_time <= t1->p_time)
                        return 1; /* Single Item, Schedulable */
                else
                        return -1; /* If(C>T), U>1, RT Test Fails */
        else {
                while(t1!=NULL)
                {
  
		rtp +=t1->c_time;
                t2=t1;
                t1=t1->cpuq;
                }

              t1 = *list;

                if(rtp > t2->p_time) {
                        pr_info("Debug 1\n");
                        return -1;/* Test Failed */
                }
                while(1)
                {
                        rtn = t2->c_time;
                        
                        for(t1=*list; t1!=t2; t1=t1->cpuq)
                        {
                          
                                cel = rtp/t1->p_time;

                                cel += (((rtp % t1->p_time)>0)?1:0);
                                rtn += cel*t1->c_time;
                        
                        }

                        if(rtn > t2->p_time)
                                return -1;

                        if (rtn == rtp)
                                return 1;

                        rtp = rtn;

                }
        }
        return 0;
}

/*
 * Priority based on time period
 */
int insert_cpu_queue_priority(sched_queue *node, sched_queue **cpu)
{
	sched_queue *n1, *n2;
	if(*cpu==NULL) {
		node->cpuq=NULL;
		*cpu=node;
		
		
		return 0;	
	}
	
	n1=*cpu;
	n2=n1->cpuq;

	/* If first nodes to be replaced */
	if(node->p_time < (*cpu)->p_time) {
		node->cpuq=*cpu;
		*cpu = node;
	}	
	else {
		while(n2!=NULL) { /* In between node */
	
			if(node->p_time > n2->p_time) {
				n1=n2;
				n2=n2->cpuq;
			}
			else {
				n1->cpuq = node;
				node->cpuq = n2;
				return 0;
			}
		
		}
		/* Last Node to be added */
		n1->cpuq=node;
		node->cpuq=NULL;
	}

return 0;
}



int insert_cpu_queue(sched_queue *node, sched_queue **cpu)
{
		
	if(node == NULL)
		return -1; //Err
	if(*cpu==NULL) {
		*cpu = node;
		(*cpu)->cpuq=NULL;
	}
	else {
		sched_queue *list = *cpu;
		while(list->cpuq!=NULL);
		list->cpuq = node;
		node->cpuq=NULL;
	}
return 0;
}



struct cpu_util{
	int util;
	int cpu;
};

struct cpu_util util_cpu[4];


/* 	change to heap/tree
*	Worst Fit Decreasing
*
*/

void bsort(struct cpu_util *arr, int n)
{
	int i, j, t, cpu;
	
	for (i = 0; i < n-1; i++)      
		for (j = 0; j < n-i-1; j++) 
			if (arr[j].util > arr[j+1].util) {
				t = arr[j].util;
				cpu = arr[j].cpu;
				arr[j].util = arr[j+1].util;
				arr[j].cpu = arr[j+1].cpu;
				arr[j+1].cpu = cpu;
				arr[j+1].util = t;					
			}
}


/* Global Queue runqueue
 * Assign temporary cpu queues 
 * for RT-Test
 */


int wfd_packing(void)
{
	sched_queue *list = runqueue;
	int ret=1;
	util_cpu[0].cpu=0;
	util_cpu[1].cpu=1;
	util_cpu[2].cpu=2;
	util_cpu[3].cpu=3;
	util_cpu[0].util=0;
	util_cpu[1].util=0;
	util_cpu[2].util=0;
	util_cpu[3].util=0;


		while(list!=NULL) {
		bsort(util_cpu, 4);	
 		
		util_cpu[0].util +=list->utils;
		
		switch(util_cpu[0].cpu) {
			case 0:
				insert_cpu_queue_priority(list, &cpu0); 
				break;
			case 1:
				insert_cpu_queue_priority(list, &cpu1); 
				break;
			case 2:
				insert_cpu_queue_priority(list, &cpu2); 
				break;
			case 3:
				insert_cpu_queue_priority(list, &cpu3); 
				break;
		}
		list = list->next;
	}
	
	if((ret=is_schedulable(&cpu0))!=1) {
		pr_info("%s: Core 1 Task Set Un-schedulable\n", __func__);
		print_list(cpu0, 1);
		goto fail;
	}
	if((ret=is_schedulable(&cpu1))!=1) {
		pr_info("%s: Core 2 Task Set Un-schedulable\n", __func__);
		print_list(cpu1, 2);
		goto fail;
	}
	if((ret=is_schedulable(&cpu2))!=1) {
		pr_info("%s: Core 3 Task Set Un-schedulable\n", __func__);
		print_list(cpu2, 3);
		goto fail;
	}
	
	if((ret=is_schedulable(&cpu3))!=1) {
		pr_info("%s: Core 4 Task Set Un-schedulable\n", __func__);
		print_list(cpu3, 4);
		goto fail;
	}

	pr_info("%s: All cores schedulable\n", __func__);

fail:				
	return ret;
}



int insert_q_descending(sched_queue **node)
{
	sched_queue *n1, *n2;
	if(runqueue==NULL) {
		runqueue=*node;
		
		return 0;	
	}
	
	n1=runqueue;
	n2=runqueue->next;

	if((*node)->utils > runqueue->utils) {
		(*node)->next=runqueue;
		runqueue = *node;
	}	
	else {
		while(n2!=NULL) {
	
			if((*node)->utils < n2->utils) {
				n1=n2;
				n2=n2->next;
			}
			else {
				n1->next = (*node);
				(*node)->next = n2;
				return 0;
			}
		
		}
		n1->next=*(node);
		(*node)->next=NULL;
	}

return 0;
}


int insert_q_ascending(sched_queue **node)
{
	sched_queue *n1, *n2;
	if(runqueue==NULL) {
		runqueue=*node;
		
		return 0;	
	}
	
	n1=runqueue;
	n2=runqueue->next;

	if((*node)->utils < runqueue->utils) {
		(*node)->next=runqueue;
		runqueue = *node;
	}	
	else {
		while(n2!=NULL) {
	
			if((*node)->utils > n2->utils) {
				n1=n2;
				n2=n2->next;
			}
			else {
				n1->next = (*node);
				(*node)->next = n2;
				return 0;
			}
		
		}
		n1->next=*(node);
		(*node)->next=NULL;
	}

return 0;
}


int delete_node(pid_t pid)
{
	sched_queue *n1, *n2;
	if(runqueue==NULL)
		return -1;

	n1=runqueue;
	n2=n1->next;

	if(n1->pid == pid) 
	{
		kfree(n1);
		runqueue =n2;
		n1 = NULL;
		return 0;

	}
	while(n1->next!=NULL) {
		if(n1->pid ==pid) {
		n2->next = n1->next;
		kfree(n1);
		return 0;	
	}
		n2 = n1;
		n1 = n1->next;
	}
	if(n1->pid==pid)
	{
		n2->next=NULL;
		kfree(n1);
		return 0;
	}

	return -1;

}


int get_process_affinity(pid_t pid)
{

	char cpubuf[10]={0};
	int ret=-1;
	unsigned long int affinity=0;

	struct  cpumask mask_print;

	if(sched_getaffinity(pid,&mask_print)<0)
	{
		printk(KERN_ALERT "Cannot get Affinity\n");
	}

	else
	{
		cpumask_scnprintf(cpubuf,10,&mask_print);
		pr_info("Affinity Before: %s\n",cpubuf); // prints in hex
	}

	ret = kstrtoul(cpubuf,16,&affinity); // converts hex string into unsigned long

	if(ret==0)
	{
		pr_info("Converted affinity [%ld]\n",affinity);
	}

	else
		affinity=0;

	return 0;

}



/************ SET PROCESSOR AFFINITY****************/

int set_process_affinity(int cpu, pid_t pid)
{


	int ret=-1;
	struct  cpumask mask;


	memset(&mask,0,sizeof(mask));
	cpumask_test_and_set_cpu(cpu,&mask);
	ret=sched_setaffinity(pid,&mask);

	if(ret!=0) {
		pr_err("%s,Err: %d\n",__func__, ret);
		return -1;

}
	return 0;
}

int assign_task_core(sched_queue *cpu, int core)
{
	int ret=0;
	sched_queue *head = cpu;
	if(head==NULL)
		return -1;

	while(head!=NULL) {
		ret = set_process_affinity(core, head->pid);
		head = head->cpuq;
	}
		
return ret;
}
