#include "common.h"
#include "string.h"
#include "x86/x86.h"
#include "proc/elf.h"
#include "proc/proc.h"

static TCB tcb[NR_THREAD], *tq_wait = NULL, *tq_blocked = NULL;

HANDLE apply_udir();
PDE *get_udir(HANDLE);
PDE *load_udir(HANDLE);

uint32_t cur_esp;
uint32_t cur_thread, cur_proc;
uint32_t tmp_stack[4096];

void show_queue(int id)
{
	TCB *tmp = NULL;

	if(id == -1)
	{
		printk("running: (%d, %d)\n", cur_thread, cur_proc);
		return;
	}

	if(id == 0)
		tmp = tq_wait;
	else
		tmp = tq_blocked;

	printk("%s chain:", id ? "blocked" : "wait");
	while(tmp != NULL)
	{
		printk("(%d, %d) -- ", tmp->tid, tmp->ppid);
		tmp = tmp->next;
	}
	printk("\n");
}

void add_run(HANDLE hThread)
{
	assert(hThread < NR_THREAD);
	/* record current context information */
	cur_thread = hThread;
	cur_proc = tcb[hThread].ppid;
	cur_esp = tcb[hThread].kesp;
	tcb[hThread].state = TS_RUN;

	/* remove from wait queue */
	TCB *tmp = tq_wait;
	if(tmp->tid == hThread)
	{
		tq_wait = tq_wait->next;
	}
	else
	{
		while(tmp != NULL && tmp->next->tid != hThread)
		{
			tmp = tmp->next;
		}

		if(tmp)
			tmp->next = tcb[hThread].next;
	}

	/* remove from blocked queue */
	tmp = tq_blocked;
	if(tmp->tid == hThread)
	{
		tq_blocked = tq_blocked->next;
	}
	else
	{
		while(tmp != NULL && tmp->next != &tcb[hThread])
		{
			tmp = tmp->next;
		}

		if(tmp)
			tmp->next = tcb[hThread].next;
	}

	tcb[hThread].next = NULL;
}

void add_wait(HANDLE hThread)
{
	assert(hThread < NR_THREAD);

	if(tcb[hThread].state == TS_UNALLOCED)
		assert(0);

	if(tcb[hThread].state == TS_WAIT)
		return;

	if(tcb[hThread].state == TS_BLOCKED)
	{
		/* remove from blocked queue */
		TCB *tmp = tq_blocked;
		assert(tmp != NULL);
		while(tmp->next != &tcb[hThread])
		{
			tmp = tmp->next;
		}

		tmp->next = tcb[hThread].next;
	}

	/* add to wait queue */
	TCB *tmp = tq_wait;
	if(tmp == NULL)
	{
		tq_wait = &(tcb[hThread]);
		tcb[hThread].next = NULL;
	}
	else
	{
		while(tmp->next != NULL && tmp->next->tp <= tcb[hThread].tp)
		{
			tmp = tmp->next;
		}

		tcb[hThread].state = TS_WAIT;
		tcb[hThread].next = tmp->next;
		tmp->next = &(tcb[hThread]);
	}
}

void add_block(HANDLE hThread)
{
	assert(hThread < NR_THREAD);

	if(tcb[hThread].state == TS_UNALLOCED)
		assert(0);

	if(tcb[hThread].state == TS_BLOCKED)
		return;

	if(tcb[hThread].state == TS_WAIT)
	{
		/* remove from wait queue */
		TCB *tmp = tq_wait;
		assert(tmp != NULL);
		while(tmp->next != &tcb[hThread])
		{
			tmp = tmp->next;
		}

		tmp->next = tcb[hThread].next;
	}

	/* add to wait queue */
	TCB *tmp = tq_blocked;
	if(tmp == NULL)
	{
		tq_blocked = &(tcb[hThread]);
		tcb[hThread].next = NULL;
	}
	else
	{
		while(tmp->next != NULL && tmp->next->tp <= tcb[hThread].tp)
		{
			tmp = tmp->next;
		}

		tcb[hThread].state = TS_BLOCKED;
		tcb[hThread].next = tmp->next;
		tmp->next = &(tcb[hThread]);
	}
}

void init_thread()
{
	//TODO:init the thread control blocks
	cur_esp = (uint32_t)(tmp_stack) + 4096;

	tq_wait = tq_blocked = NULL;

	int i;
	for(i = 0; i < NR_THREAD; i++)
	{
		tcb[i].state = TS_UNALLOCED;
		tcb[i].next = NULL;
		tcb[i].kesp = (uint32_t)(tcb[i].stack) + USER_KSTACK_SIZE;
	}
}

/* apply thread handle */
HANDLE apply_th()
{
	int i;
	for(i = 0; i < NR_THREAD; i++)
	{
		if(tcb[i].state == TS_UNALLOCED)
			return i;
	}
	assert(0);
	return INVALID_HANDLE_VALUE;
}

HANDLE create_thread(HANDLE hProc, ThreadAttr *pta)
{
	/* apply a thread handle */
	HANDLE hThread = apply_th();

	/* init the tcb info */
	tcb[hThread].tf.eip = pta->entry;
	tcb[hThread].kesp = (uint32_t)(tcb[hThread].stack) + USER_KSTACK_SIZE;
	tcb[hThread].tp = pta->thread_prior;
	tcb[hThread].tid = hThread;
	tcb[hThread].ptid = pta->ptid;
	tcb[hThread].ppid = hProc;
	tcb[hThread].state = TS_WAIT;
	tcb[hThread].timescales = 0;

	/* construct pesudo trapframe */
	set_usrtf(pta->entry, &tcb[hThread].tf);

	/* add this thread to wait queue */
	add_wait(hThread);
	
	return hThread;
}

/* run this thread */
void enter_thread(HANDLE hThread)
{
	assert(hThread < NR_THREAD);

	/* record current context information */
	add_run(hThread);
	
	void *tf = (void *)USER_STACK_ADDR - sizeof(TrapFrame);
	memcpy(tf, &tcb[hThread].tf, sizeof(TrapFrame));
	env_run(tf);
}

void update_eip(HANDLE hThread, uint32_t eip)
{
	assert(hThread < NR_THREAD);
	tcb[hThread].tf.eip = eip;
}

void switch_thread(TrapFrame *tf)
{
	/* TODO: choose a thread from wait queue by priority
	 * save current thread's trapframe :
	 *		memcpy(cur_thread.tf, tf, sizeof(tf))
	 * load new thread's trapframe :
	 *		memcpy(tf, new_thread.tf, sizeof(tf))
	 * set cur_esp and cur_handle
	 */

	tcb[cur_thread].timescales ++;
	tcb[cur_thread].tf.eip = tf->eip;
	pcb_time_plus(cur_proc);

	HANDLE new_thread = cur_thread;
	TCB *tmp = tq_wait;
	while(tmp != NULL)
	{
		if(tmp->tp <= tcb[cur_thread].tp)
		{
			new_thread = tmp->tid;
			break;
		}
		tmp = tmp->next;
	}

	if(new_thread != cur_thread)
	{
		uint32_t old_thread = cur_thread;
		add_wait(cur_thread);
		add_run(new_thread);
		/* store TrapFrame information */
		memcpy(&(tcb[old_thread].tf), tf, sizeof(TrapFrame));
		memcpy(tf, &(tcb[new_thread].tf), sizeof(TrapFrame));

		load_udir(tcb[new_thread].ppid);
	}
	
}

void copy_thread_tree(HANDLE hSrc, HANDLE hDst)
{
	assert(cur_thread < NR_THREAD);

	int i;
	for(i = 0; i < NR_THREAD; i++)
	{
		if(tcb[i].ppid == hSrc)
		{
			HANDLE hnew = apply_th();
			tcb[hnew] = tcb[i];
			tcb[hnew].tid = hnew;
			tcb[hnew].ppid = hDst;
			tcb[hnew].kesp = (uint32_t)(tcb[hnew].stack) + USER_KSTACK_SIZE;
			if(tcb[i].state == TS_RUN || tcb[i].state == TS_WAIT)
			{
				add_wait(hnew);
			}
			else if(tcb[i].state == TS_BLOCKED)
			{
				add_block(hnew);
			}

			if(i == cur_thread)
			{
				tcb[hnew].tf.eax = -1;
				tcb[i].tf.eax = hDst;
				printk("\n==[%d, %x, %x]==\n", hnew, tcb[hnew].tf.eip, tcb[i].tf.eip);
			}
		}
	}
}

/* new syscall:
 * 1. fork()
 * 2. pthread_t(FuncEntry)
 * 3. join() // block current thread
 * 4. wait(hThread)
 * 5. sleep(ms);
 * 6. exit();
 */
