#include "common.h"
#include "x86/x86.h"
#include "proc/elf.h"
#include "proc/proc.h"

void init_page();
void init_mm();
void init_8259();
void init_timer();
void init_idt();
void init_keyboard();
void init_serial();
void init_video();
void init_disk();
void init_font();
void test_printk();
void init_segment();
void init_proc();
void init_thread();
void init_wlist();
void init_fs();
void init_ufs();
void init_console();
void load_game();
void init_cond();

void enter_proc(HANDLE);

int shell(TrapFrame *tf);

extern uint32_t _stack_end_;

void init_prev()
{
	init_page();
	asm volatile("movl %0, %%esp"::"a"(&_stack_end_));
	asm volatile("jmpl *%0"::"r"(init_cond));
}

void init_cond()
{
	init_segment();

	init_idt();
	init_serial();
	init_8259();
	init_timer();
	init_keyboard();
	init_video();
	sti();

	init_mm();
	init_proc();
	init_thread();
	init_wlist();
	init_font();
	init_disk();
	init_fs();
	init_ufs();
	init_console();

	/* create idle process */
	ProcAttr pa = {4};
	HANDLE hProc = create_proc("/idle", &pa);
	enter_proc(hProc);

	while(1);
}
