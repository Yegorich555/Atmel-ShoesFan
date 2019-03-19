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
#define IO_ADC_InLed 1 //portb.2 adc1
#define IO_OutFan B, 4

#define OnTime 3600 //s
#define AdcOnValue 640 //adc-discrets 0...1024

uint16_t t_ms;
uint16_t t_sec;

uint8_t latest_pin;

#if DEBUG
#define USOFT_BAUD 4800
#define USOFT_IO_TX B, 1
#define USOFT_RXEN 0
#include <uart_soft.h>
void _puts(char *str)
{
	#if DEBUG
	usoft_putString((unsigned char *)str);
	usoft_putCharf(0x0D);
	#endif
}
#endif

#define REF_AVCC (0<<REFS0) // reference = AVCC
#define REF_INT  (1<<REFS0) // internal reference 1.1 V

//todo check ADLAR & ADCSRA
#define ADC_VREF_TYPE REF_AVCC//((1<<REFS0) | (0<<ADLAR))

void adc_init(void)
{
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); // prescaler 64 => 125 kHz
	DIDR0|=(0<<ADC0D) | (0<<ADC2D) | (0<<ADC3D) | (1<<ADC1D); //disable digital input for selected ADC
}

// Read the AD conversion result
unsigned int read_adc(unsigned char adc_input)
{
	ADMUX=adc_input | ADC_VREF_TYPE;
	// Delay needed for the stabilization of the ADC input voltage
	_delay_us(10);
	// Start the AD conversion
	ADCSRA|=(1<<ADSC);
	// Wait for the AD conversion to complete
	while ((ADCSRA & (1<<ADIF))==0);
	ADCSRA|=(1<<ADIF);
	return ADCW;
}

int main(void)
{
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); // prescaler 64 => 125 kHz
	DIDR0|=(0<<ADC0D) | (0<<ADC2D) | (0<<ADC3D) | (1<<ADC1D); //disable digital input for selected ADC

	TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);
	TCNT0=0x6A;
	TIMSK0=(0<<OCIE0B) | (0<<OCIE0A) | (1<<TOIE0);
	
	io_set(DDR, IO_OutFan);
	io_set(DDR, IO_OutLed);
	
	asm_sei();
	
	#if DEBUG
	usoft_init();
	_puts("start");
	
	for (int i=0;i<3;++i){
		io_setPort(IO_OutFan); delay_ms(300);
		io_resetPort(IO_OutFan); delay_ms(300);
	}
	
	while(1)
	{
		io_setPort(IO_OutLed);
		_delay_ms(1);
		int adcMesure = read_adc(IO_ADC_InLed); //todo calc+time
		io_resetPort(IO_OutLed);
		
		if (adcMesure > AdcOnValue)
		{
			io_setPort(IO_OutFan);
		}
		else
		{
			io_resetPort(IO_OutFan);
		}
		
		usoft_putString("adc:");

		int at = adcMesure / 1000;
		usoft_putChar(48 + at);
		adcMesure = adcMesure - at * 1000;
		
		at = adcMesure /100;
		usoft_putChar(48 + at);
		adcMesure = adcMesure - at * 100;
		
		at = adcMesure /10;
		usoft_putChar(48 + at);
		adcMesure = adcMesure - at * 10;

		usoft_putChar(48 + adcMesure);
		usoft_putChar(0x0D);
		
		delay_ms(500);
	}
	#else
	bool isFanOn = false;
	while (1)
	{
		wdt_reset();
		wdt_enable(WDTO_250MS);
		
		io_setPort(IO_OutLed);
		delay_ms(1);
		int adcMesure = read_adc(IO_ADC_InLed); //todo calc+time
		io_resetPort(IO_OutLed);
		
		if (!isFanOn && adcMesure > AdcOnValue) 
		{
			isFanOn = true;
			t_ms = 0;
			t_sec = 0;
			io_setPort(IO_OutFan);
			delay_ms(100);
		}
		else
		{
			io_resetPort(IO_OutFan);
			isFanOn = false;
		}
		delay_ms(1);
	}
	#endif
}

#ifndef DEBUG
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
#endif