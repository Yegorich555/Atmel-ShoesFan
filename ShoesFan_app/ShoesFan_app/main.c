/*
* ShoesFan_app.c
*
* Created: 18.02.2019 16:35:14
* Author : Yegor
*/

#define F_CPU 9600000UL
#include <avr/io.h>
#include <extensions.h> //file from https://github.com/Yegorich555/Atmel-Library
#include <avr/wdt.h>
#include <avr/eeprom.h>

#define IO_OutLed B, 0
#define IO_ADC_InLed 1 //portb.2 adc1
#define IO_OutFan B, 4

#define OnTime 3600 //time-work for fan in sec

static uint16_t EEMEM _e_adc_dOnValue = 10; //storing in EEPROM
volatile uint16_t adc_dOnValue;  //difference in adc-discrets 0...1024 when fan should work

void wdt_restart()
{
	#ifndef DEBUG
	wdt_reset();
	wdt_enable(WDTO_250MS);
	#endif
}

volatile uint16_t t_ms, t_sec;

#if DEBUG
#define USOFT_BAUD 4800
#define USOFT_IO_TX B, 1
#define USOFT_RXEN 0
#include <uart_soft.h>
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
	
	uint16_t v = eeprom_read_word(&_e_adc_dOnValue);
	if (v != 0xFFFF) adc_dOnValue = v;
	
	#if DEBUG
	delay_ms(1000);
	usoft_init();
	usoft_putStringf("start:");
	usoft_putUInt(adc_dOnValue);
	usoft_putCharf(0x0D);
	
	for (int i=0; i<3; ++i){
		io_setPort(IO_OutFan); delay_ms(300);
		io_resetPort(IO_OutFan); delay_ms(300);
	}
	#endif
	
	bool isFanOn = true;
	int adcBefore = read_adc(IO_ADC_InLed); // first reading for ADC stabilization
	while (1)
	{
		wdt_restart();
		
		io_setPort(IO_OutLed);
		delay_ms(1);
		
		int adcMesure = 0;
		const int adcCount = 5;
		for (uint8_t i = 0; i < adcCount; ++i)
		{
			_delay_us(100);
			adcMesure += read_adc(IO_ADC_InLed);
		}
		adcMesure = adcMesure/adcCount;
		io_resetPort(IO_OutLed);

		if (adcBefore > adcMesure && (adcBefore - adcMesure) > adc_dOnValue)
		{
			if (!isFanOn)
			{
				t_ms = 0;
				t_sec = 0;
				isFanOn = true;
				io_setPort(IO_OutFan);
				#if DEBUG
				usoft_putStringf("ON\n");
				#endif
			}
		}
		else if (adcBefore < adcMesure && (adcMesure - adcBefore) > adc_dOnValue)
		{
			io_resetPort(IO_OutFan);
			#if DEBUG
			if (isFanOn)
				usoft_putStringf("OFF\n");
			#endif
			isFanOn = false;
		}
		adcBefore = adcMesure;
		
		#if DEBUG
		usoft_putStringf("adc:");
		usoft_putUInt(adcMesure);
		usoft_putCharf(0x0D);
		#endif
		
		for (uint8_t i = 0; i< 5; ++i) //delay 500 ms between cycles
		{
			wdt_restart();
			delay_ms(100);
		}
	}
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