#include "common.h"
#include "x86/x86.h"

void init_8259();
void init_timer();
void init_idt();
void init_keyboard();
void init_serial();
void init_video();
void init_disk();
void init_font();
void test_printk();
int exec(const char* path);

int main()
{
	init_video();
	init_serial();
	init_timer();
	init_idt();
	init_keyboard();
	init_8259();
	init_disk();
	init_font();
	test_printk();
	sti();
	exec("game");
	while(1);
	return 0;
}