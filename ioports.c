#include <stdint.h>
#include "avr.h"
#include "ioports.h"

#if (PWM_OUTPUT_PORTS > 0)
extern uint16_t	pwm_timer1_get_oc1a(void);
extern void		pwm_timer1_set_oc1a(uint16_t value); // prevent include loop
static uint16_t get_pwm_oc1a_16(void)
{
	uint16_t raw;

	raw = pwm_timer1_get_oc1a();
	return((raw << 6) | (raw & 0x003f));
}
static void		set_pwm_oc1a_16(uint16_t value)
{
	return(pwm_timer1_set_oc1a(value >> 6)); // 16 bits -> 10 bits resolution
}
#endif

#if (PWM_OUTPUT_PORTS > 1)
extern uint16_t	pwm_timer1_get_oc1b(void);
extern void		pwm_timer1_set_oc1b(uint16_t value);
static uint16_t get_pwm_oc1b_16(void)
{
	uint16_t raw;

	raw = pwm_timer1_get_oc1b();
	return((raw << 6) | (raw & 0x003f));
}
static void		set_pwm_oc1b_16(uint16_t value)
{
	return(pwm_timer1_set_oc1b(value >> 6)); // 16 bits -> 10 bits resolution
}
#endif

#if (PWM_OUTPUT_PORTS > 2)
extern uint16_t	pwm_timer1_get_oc1d(void);
extern void		pwm_timer1_set_oc1d(uint16_t value);
static uint16_t get_pwm_oc1d_16(void)
{
	uint16_t raw;

	raw = pwm_timer1_get_oc1d();
	return((raw << 4) | (raw & 0x000f));
}
static void set_pwm_oc1d_16(uint16_t value)
{
	return(pwm_timer1_set_oc1d(value >> 4)); // 16 bits -> 12 bits resolution
}
#endif

#if (PLAIN_OUTPUT_PORTS > 0)
extern uint16_t	timer0_get_compa(void);
extern void		timer0_set_compa(uint16_t);
static uint16_t get_timer_0a_16(void)
{
	uint16_t raw;

	raw = timer0_get_compa();
	return((raw << 8) | (raw & 0x00ff)); // 8 bits -> 16 bits resolution
}
static void set_timer_0a_16(uint16_t value)
{
	value >>= 8;

	if(value < 4)
		value = 0;

	timer0_set_compa(value);
}
#endif

#if (PLAIN_OUTPUT_PORTS > 1)
extern uint16_t	timer0_get_compb(void);
extern void		timer0_set_compb(uint16_t);
static uint16_t get_timer_0b_16(void)
{
	uint16_t raw;

	raw = timer0_get_compb();
	return((raw << 8) | (raw & 0x00ff)); // 8 bits -> 16 bits resolution
}
static void set_timer_0b_16(uint16_t value)
{
	value >>= 8;

	if(value < 4)
		value = 0;

	timer0_set_compb(value);
}
#endif

adcport_t temp_ports[TEMP_PORTS] = 
{
	{							// internal temp sensor
		{ 1, 1, 1, 1, 1, 1 }	// mux = adc11 = 111111
	},
#if (BOARD == 0)
	{							// pa4 pin 14
		{ 1, 1, 0, 0, 0, 0 },	// mux = adc3
	},
#endif
#if (BOARD == 1)
	{							// pa4 pin 14
		{ 1, 1, 0, 0, 0, 0 },	// mux = adc3
	},
#endif
};

adcport_t analog_ports[ANALOG_PORTS] = 
{
#if (BOARD == 0)
	{							// pa7 pin 11 adc input 1
		{ 0, 1, 1, 0, 0, 0 },	// mux = adc6 = 000110
	},
	{							// pa5 pin 13 adc input 2
		{ 0, 0, 1, 0, 0, 0 },	// mux = adc4
	},
#endif
#if (BOARD == 1)
	{							// pa7 pin 11 adc input 1
		{ 0, 1, 1, 0, 0, 0 },	// mux = adc6 = 000110
	},
#endif
};

inport_t input_ports[INPUT_PORTS] =
{
#if (BOARD == 0)
	{ &PORTB, &PINB, &DDRB, 6, &PCMSK1, PCINT14, PCIE1 },	// b6
	{ &PORTA, &PINA, &DDRA, 6, &PCMSK0, PCINT6,  PCIE1 },	// a6
	{ &PORTB, &PINB, &DDRB, 0, &PCMSK1, PCINT8,  PCIE0 },	// b0
	{ &PORTA, &PINA, &DDRA, 1, &PCMSK0, PCINT1,  PCIE1 }	// a1
#endif
#if (BOARD == 1)
	{ &PORTB, &PINB, &DDRB, 6, &PCMSK1, PCINT14, PCIE1 },	// b6
	{ &PORTA, &PINA, &DDRA, 6, &PCMSK0, PCINT6,  PCIE1 },	// a6
#endif
};

pwmport_t output_ports[OUTPUT_PORTS] =
{
#if (BOARD == 0)
	{ &PORTB, &DDRB, 1, &get_pwm_oc1a_16,		&set_pwm_oc1a_16	},	// b1	output 1
	{ &PORTB, &DDRB, 3, &get_pwm_oc1b_16,		&set_pwm_oc1b_16	},	// b3	output 2
	{ &PORTB, &DDRB, 5,	&get_pwm_oc1d_16,		&set_pwm_oc1d_16	},	// b5	output 3
	{ &PORTB, &DDRB, 2,	&get_timer_0a_16,		&set_timer_0a_16	},	// b2	output 4
#endif
#if (BOARD == 1)
	{ &PORTB, &DDRB, 1, &get_pwm_oc1a_16,		&set_pwm_oc1a_16	},	// b1	output 1
	{ &PORTB, &DDRB, 3, &get_pwm_oc1b_16,		&set_pwm_oc1b_16	},	// b3	output 2
	{ &PORTB, &DDRB, 5,	&get_pwm_oc1d_16,		&set_pwm_oc1d_16	},	// b5	output 3
	{ &PORTB, &DDRB, 2,	&get_timer_0a_16,		&set_timer_0a_16	},	// b2	output 4
#endif
};

outport_t internal_output_ports[INTERNAL_OUTPUT_PORTS] =
{
#if (BOARD == 0)
	{ &PORTA, &DDRA, 3 },	// a3	input sense
	{ &PORTB, &DDRB, 4 },	// b4	command sense
#endif
#if (BOARD == 1)
	{ &PORTB, &DDRB, 4 },	// b4	command sense
	{ &PORTA, &DDRA, 3 },	// a3	input sense
#endif
};
