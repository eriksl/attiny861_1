#include <stdint.h>
#include <avr/io.h>

#include "ioports.h"

const adcport_t adc_ports[ADC_PORTS] = 
{
	{							// pa7 pin 11
		{ 0, 0, 0 },			// Vref = Vcc
		{ 0, 1, 1, 0, 0, 0 },	// mux = adc6
	},
	{							// pa5 pin 13
		{ 0, 0, 0 },			// Vref = Vcc
		{ 0, 0, 1, 0, 0, 0 },	// mux = adc4
	},
	{							// pa4 pin 14
		{ 0, 0, 0 },			// Vref = Vcc
		{ 1, 1, 0, 0, 0, 0 },	// mux = adc3
	},
	{							// internal temp sensor
		{ 0, 1, 0, },			// Vref = internal 1.1V reference
		{ 1, 1, 1, 1, 1, 1 }	// mux = adc11
	}
};

const ioport_t input_ports[INPUT_PORTS] =
{
	{ &PORTB, &PINB, &DDRB, 6 },
	{ &PORTA, &PINA, &DDRA, 6 },
	{ &PORTB, &PINB, &DDRB, 0 },
	{ &PORTA, &PINA, &DDRA, 1 }
};

const ioport_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, &PINB, &DDRB, 2 },
	{ &PORTB, &PINB, &DDRB, 4 },
	{ &PORTA, &PINA, &DDRA, 3 },
	{ &PORTA, &PINA, &DDRA, 4 }
};

const pwmport_t pwm_ports[PWM_PORTS] =
{
	{ &PORTB, &DDRB, 1, &TCCR1A, COM1A1, COM1A0, FOC1A, PWM1A, &TC1H, &OCR1A },
	{ &PORTB, &DDRB, 3, &TCCR1A, COM1B1, COM1B0, FOC1B, PWM1B, &TC1H, &OCR1B },
	{ &PORTB, &DDRB, 5, &TCCR1C, COM1D1, COM1D0, FOC1D, PWM1D, &TC1H, &OCR1D }
};
