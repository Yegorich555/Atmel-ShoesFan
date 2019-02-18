/*
* ShoesFan_app.c
*
* Created: 18.02.2019 16:35:14
* Author : Yegor
*/

#define F_CPU 9600000UL
#include <avr/io.h>
#include <extensions.h>
#include <avr/wdt.h>

#define IO_OutLed B, 0
#define IO_InLed B, 2
#define IO_OutFan B, 4

#define OnTime 3600 //s


uint16_t t_ms;
uint16_t t_sec;

uint8_t latest_pin;
int main(void)
{
	TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);
	TCNT0=0x6A;
	TIMSK0=(0<<OCIE0B) | (0<<OCIE0A) | (1<<TOIE0);
	
	io_set(DDR, IO_OutFan);
	io_set(DDR, IO_OutLed);
	
	uint8_t pin = 0;
	
	asm_sei();
	while (1)
	{
		wdt_reset();
		wdt_enable(WDTO_250MS);
	
		io_setPort(IO_OutLed);
		delay_ms(1);
		pin = io_getPin(IO_InLed);
		if (pin != latest_pin){ //todo wait for 500ms
			if (pin) {
				t_ms = 0;
				t_sec = 0;
				io_setPort(IO_OutFan);
			} else {
				io_resetPort(IO_OutFan);
			}
				
		}
		io_resetPort(IO_OutLed);
		delay_ms(1);
	}
}

ISR(TIM0_OVF_vect) //timer interrupts each 1 ms
{
	TCNT0=0x6A;
	++t_ms;
	if (t_ms >= 1000)
	{
		++t_sec;
		t_ms = 0;
	}
	if (t_sec >= OnTime)
	{
		t_sec = OnTime;
		io_resetPort(IO_OutFan);
	}
}