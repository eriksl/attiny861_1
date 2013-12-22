#include <stdint.h>
#include "avr.h"

#include "ioports.h"
#include "pwm_timer1.h"

static uint8_t cs1[4];

void pwm_timer1_init(uint8_t prescaler)
{
	uint8_t temp, mask;

	if(prescaler > 15)
		return;

	cs1[0] = (prescaler & (1 << 0)) >> 0;
	cs1[1] = (prescaler & (1 << 1)) >> 1;
	cs1[2] = (prescaler & (1 << 2)) >> 2;
	cs1[3] = (prescaler & (1 << 3)) >> 3;

	pwm_timer1_stop();

	TCCR1A =	(0 << COM1A1)	|	// compare 1a output mode
				(0 << COM1A0)	|	// compare 1a output mode
				(0 << COM1B1)	|	// compare 1b output mode
				(0 << COM1B0)	|	// compare 1b output mode
				(0 << FOC1A)	|	// force compare 1a output
				(0 << FOC1B)	|	// force compare 1b output
				(0 << PWM1A)	|	// enable pwm compare 1a
				(0 << PWM1B);		// enable pwm compare 1b

	TCCR1B =	(0 << PWM1X)	|	// enable pwm inversion mode
				(0 << PSR1)		|	// reset prescaler
				(0 << DTPS11)	|	// dead time prescaler
				(0 << DTPS10)	|	// dead time prescaler
				(0 << CS13)		|	// clock select bit 3
				(0 << CS12)		|	// clock select bit 2
				(0 << CS11)		|	// clock select bit 1
				(0 << CS10);		// clock select bit 0

	TCCR1C =	(0 << COM1A1S)	|	// shadow compare 1a output mode
				(0 << COM1A0S)	|	// shadow compare 1a output mode
				(0 << COM1B1S)	|	// shadow compare 1b output mode
				(0 << COM1B0S)	|	// shadow compare 1b output mode
				(0 << COM1D1)	|	// compare 1d output mode
				(0 << COM1D0)	|	// compare 1d output mode
				(0 << FOC1D)	|	// force compare d output
				(0 << PWM1D);		// enable pwm compare 1d

	TCCR1D =	(0 << FPIE1)	|	// enable fault protection interrupt
				(0 << FPEN1)	|	// enable fault protection mode
				(0 << FPNC1)	|	// enable fault protection noise canceler
				(0 << FPES1)	|	// fault protection edge select
				(0 << FPAC1)	|	// enable fault protection analog comparator
				(0 << FPF1)		|	// fault protection interrupt flag
				(0 << WGM11)	|	// wave mode, 00 = normal / fast pwm, 01 = correct pwm, 10/11 = pwm6
				(1 << WGM10);		//		top = ocr1c, overflow at bottom

	TCCR1E	=	0;					// output compare pwm6 override bits

	PLLCSR =	(0 << LSM)		|	// enable low speed mode (pll / 2)
				(0 << 6)		|	// reserved
				(0 << 5)		|	// reserved
				(0 << 4)		|	// reserved
				(0 << 3)		|	// reserved
				(0 << PCKE)		|	// high speed mode (32/64 Mhz)
				(0 << PLLE)		|	// pll enable
				(0 << PLOCK);		// pll lock detector

	mask =		(1 << OCIE1D)	|	// enable interrupt output compare 1d
				(1 << OCIE1A)	|	// enable interrupt output compare 1a
				(1 << OCIE1B)	|	// enable interrupt output compare 1b
				(0 << OCIE0A)	|	// enable interrupt output compare 0a
				(0 << OCIE0B)	|	// enable interrupt output compare 0b
				(1 << TOIE1)	|	// enable interrupt output overflow timer 1
				(0 << TOIE0)	|	// enable interrupt output overflow timer 0
				(0 << TICIE0);		// enable interrupt input capture timer0

	temp = TIMSK & ~mask;

	temp |=		(0 << OCIE1D)	|	// enable interrupt output compare 1d
				(0 << OCIE1A)	|	// enable interrupt output compare 1a
				(0 << OCIE1B)	|	// enable interrupt output compare 1b
				(0 << OCIE0A)	|	// enable interrupt output compare 0a
				(0 << OCIE0B)	|	// enable interrupt output compare 0b
				(0 << TOIE1)	|	// enable interrupt output overflow timer 1
				(0 << TOIE0)	|	// enable interrupt output overflow timer 0
				(0 << TICIE0);		// enable interrupt input capture timer0

	TIMSK = temp;

	TIFR =		(1 << OCF1D)	|	// output compare flag 1d
				(1 << OCF1A)	|	// output compare flag 1a
				(1 << OCF1B)	|	// output compare flag 1b
				(0 << OCF0A)	|	// output compare flag 0a
				(0 << OCF0B)	|	// output compare flag 0b
				(1 << TOV1)		|	// overflow flag timer1
				(0 << TOV0)		|	// overflow flag timer0
				(0 << ICF0);		// input capture flag timer0
}

void pwm_timer1_start(void)
{
	pwm_timer1_stop();
	pwm_timer1_reset_counter();

	TCCR1B =	(0		<< PWM1X)	|	// enable pwm inversion mode
				(1		<< PSR1)	|	// reset prescaler
				(0		<< DTPS11)	|	// dead time prescaler
				(0		<< DTPS10)	|	// dead time prescaler
				(cs1[3]	<< CS13)	|	// clock select bit 3
				(cs1[2]	<< CS12)	|	// clock select bit 2
				(cs1[1]	<< CS11)	|	// clock select bit 1
				(cs1[0]	<< CS10);		// clock select bit 0
}

void pwm_timer1_stop(void)
{
	TCCR1B =	(0		<< PWM1X)	|	// enable pwm inversion mode
				(1		<< PSR1)	|	// reset prescaler
				(0		<< DTPS11)	|	// dead time prescaler
				(0		<< DTPS10)	|	// dead time prescaler
				(0		<< CS13)	|	// clock select bit 3
				(0		<< CS12)	|	// clock select bit 2
				(0		<< CS11)	|	// clock select bit 1
				(0		<< CS10);		// clock select bit 0
}

#if (PWM_OUTPUT_PORTS > 0)
void pwm_timer1_set_oc1a(uint16_t pwm_value)
{
	if((pwm_value == 0) || (pwm_value > 0x3ff))
	{
		TC1H	= 0;
		OCR1A	= 0;
		TCCR1A 	&= ~(_BV(COM1A1) | _BV(COM1A0) | _BV(PWM1A));

		if(pwm_value == 0)
			*output_ports[0].port &= ~_BV(output_ports[0].bit);
		else
			*output_ports[0].port |= _BV(output_ports[0].bit);
	}
	else
	{
		uint8_t control_reg_temp;

		TC1H	= pwm_value >> 8;
		OCR1A	= pwm_value & 0xff;

		control_reg_temp	= TCCR1A;
		control_reg_temp	&= ~_BV(COM1A0);							//	clear on match when counting up, set on match when counting down
		control_reg_temp	|= _BV(COM1A1) | _BV(PWM1A) | _BV(FOC1A);	//	force immediate effect
		TCCR1A				= control_reg_temp;
	}
}

uint16_t pwm_timer1_get_oc1a(void)
{
	uint16_t rv;

	rv = OCR1A;
	rv |= TC1H << 8;

	if((rv == 0) && (*output_ports[0].port & _BV(output_ports[0].bit)))
		rv = 0x3ff;

	return(rv);
}
#endif

#if (PWM_OUTPUT_PORTS > 1)
void pwm_timer1_set_oc1b(uint16_t pwm_value)
{
	if((pwm_value == 0) || (pwm_value > 0x3ff))
	{
		TC1H	= 0;
		OCR1B	= 0;
		TCCR1A 	&= ~(_BV(COM1B1) | _BV(COM1B0) | _BV(PWM1B));

		if(pwm_value == 0)
			*output_ports[1].port &= ~_BV(output_ports[1].bit);
		else
			*output_ports[1].port |= _BV(output_ports[1].bit);
	}
	else
	{
		uint8_t control_reg_temp;

		TC1H	= pwm_value >> 8;
		OCR1B	= pwm_value & 0xff;

		control_reg_temp	= TCCR1A;
		control_reg_temp	&= ~_BV(COM1B0);							//	clear on match when counting up, set on match when counting down
		control_reg_temp	|= _BV(COM1B1) | _BV(PWM1B) | _BV(FOC1B);	//	force immediate effect
		TCCR1A				= control_reg_temp;
	}
}

uint16_t pwm_timer1_get_oc1b(void)
{
	uint16_t rv;

	rv = OCR1B;
	rv |= TC1H << 8;

	if((rv == 0) && (*output_ports[1].port & _BV(output_ports[1].bit)))
		rv = 0x3ff;

	return(rv);
}
#endif

#if (PWM_OUTPUT_PORTS > 2)
void pwm_timer1_set_oc1d(uint16_t pwm_value)
{
	if((pwm_value == 0) || (pwm_value > 0x3ff))
	{
		TC1H	= 0;
		OCR1D	= 0;
		TCCR1C 	&= ~(_BV(COM1D1) | _BV(COM1D0) | _BV(PWM1D));

		if(pwm_value == 0)
			*output_ports[2].port &= ~_BV(output_ports[2].bit);
		else
			*output_ports[2].port |= _BV(output_ports[2].bit);
	}
	else
	{
		uint8_t control_reg_temp;

		TC1H	= pwm_value >> 8;
		OCR1D	= pwm_value & 0xff;

		control_reg_temp	= TCCR1C;
		control_reg_temp	&= ~_BV(COM1D0);							//	clear on match when counting up, set on match when counting down
		control_reg_temp	|= _BV(COM1D1) | _BV(PWM1D) | _BV(FOC1D);	//	force immediate effect
		TCCR1C				= control_reg_temp;
	}
}

uint16_t pwm_timer1_get_oc1d(void)
{
	uint16_t rv;

	rv = OCR1D;
	rv |= TC1H << 8;

	if((rv == 0) && (*output_ports[2].port & _BV(output_ports[2].bit)))
		rv = 0x3ff;

	return(rv);
}
#endif
