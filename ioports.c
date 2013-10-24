#include <stdint.h>
#include <avr/io.h>

#include "ioports.h"

const adcport_t temp_ports[TEMP_PORTS] = 
{
	{							// internal temp sensor
		{ 1, 1, 1, 1, 1, 1 }	// mux = adc11 = 111111
	},
	{							// pa4 pin 14 adc input 3
		{ 1, 1, 0, 0, 0, 0 },	// mux = adc3
	},
};

const adcport_t analog_ports[ANALOG_PORTS] = 
{
	{							// pa7 pin 11 adc input 1
		{ 0, 1, 1, 0, 0, 0 },	// mux = adc6 = 000110
	},
	{							// pa5 pin 13 adc input 2
		{ 0, 0, 1, 0, 0, 0 },	// mux = adc4
	},
};

const ioport_t input_ports[INPUT_PORTS] =
{
	{ &PORTB, &PINB, &DDRB, 6, &PCMSK1, PCINT14, PCIE1 },	// b6
	{ &PORTA, &PINA, &DDRA, 6, &PCMSK0, PCINT6,  PCIE1 },	// a6
	{ &PORTB, &PINB, &DDRB, 0, &PCMSK1, PCINT8,  PCIE0 },	// b0
	{ &PORTA, &PINA, &DDRA, 1, &PCMSK0, PCINT1,  PCIE1 }	// a1
};

const ioport_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, &PINB, &DDRB, 2 }		// b2	output 1
};

const ioport_t internal_output_ports[INTERNAL_OUTPUT_PORTS] =
{
	{ &PORTA, &PINA, &DDRA, 3 },	// a3	input sense
	{ &PORTB, &PINB, &DDRB, 4 },	// b4	command sense
};

const pwmport_t pwm_ports[PWM_PORTS] =
{
	{ &PORTB, &DDRB, 1, &TCCR1A, COM1A1, COM1A0, FOC1A, PWM1A, &TC1H, &OCR1A },
	{ &PORTB, &DDRB, 3, &TCCR1A, COM1B1, COM1B0, FOC1B, PWM1B, &TC1H, &OCR1B },
	{ &PORTB, &DDRB, 5, &TCCR1C, COM1D1, COM1D0, FOC1D, PWM1D, &TC1H, &OCR1D }
};
