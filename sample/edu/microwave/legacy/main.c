#include "serial.h"
#include "reg.h"
#include "section.h"
#include "interrupt.h"
#include <string.h>
#include "timer.h"
#include "v850_ins.h"
#include "digital.h"

unsigned char stack_data[STACK_SIZE] __attribute__ ((section(".bss_noclr")));

/*
 * up/down/start/stop SWITCH SPECIFICATION
 *
 * check interval: 100 msec
 *
 * -------------------+-------+-----------+-----------+
 *                    | RESET | JUDGE(1)  | SWITCH-ON |
 * -------------------+-------+-----------+-----------+
 * 0->1               | JUDGE |    -      |     -     |
 * -------------------+-------+-----------+-----------+
 * 1->0               |   -   | RESET     |     -     |
 * -------------------+-------+-----------+-----------+
 * 0->0               |   -   |    -      |     -     |
 * -------------------+-------+-----------+-----------+
 * 1->1               |   -   | stay      |     -     |
 * -------------------+-------+-----------+-----------+
 * 1 keeps 0.5sec     |   -   | SWITCH-ON |     -     |
 * -------------------+-------+-----------+-----------+
 * 0 keeps 0.5sec     |   -   | -         |     -     |
 * -------------------+-------+-----------+-----------+
 * RESET-EVENT        | RESET | RESET     | RESET     |
 * -------------------+-------+-----------+-----------+
 */

static uint32 sys_time = 0;

static uint32 count = 0;
static void timer_interrupt_handler(void)
{
	volatile unsigned char *p = DIGITAL_REG_ADDR;
	count++;
	if (count > 10) {
		athrill_fputs("sys_time");
		athrill_fputi(sys_time++);
		count = 0;
	}
	return;
}

int main(void)
{
	athrill_fputs("heat_time");
	athrill_fputi(100);
	athrill_fputs("heat_method");
	athrill_fputs("hurry_up!!");
	athrill_fputs("last_time");
	athrill_fputi(80);

	timer_init(timer_interrupt_handler);
	timer_start(10000);

    while (TRUE) {
        do_idle();
    }
    return 0;
}


void bss_clear(void)
{
	unsigned char *p = &_bss_start;
	unsigned char *e = &_bss_end;
	for (;p < e; p++) {
		*p = 0;
	}
	return;
}

void data_init(void)
{
	unsigned char *p_rom = &_idata_start;
	unsigned char *e_rom = &_idata_end;
	unsigned char *p_ram = &_data_start;

	for (;p_rom < e_rom; p_ram++, p_rom++) {
		*p_ram = *p_rom;
	}
}
