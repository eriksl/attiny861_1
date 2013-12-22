#if !defined(_PWM_TIMER1_H_)
#define _PWM_TIMER1_H_

#include <stdint.h>
#include "avr.h"

enum
{
	PWM_TIMER1_PRESCALER_OFF	= 0,
	PWM_TIMER1_PRESCALER_1		= 1,
	PWM_TIMER1_PRESCALER_2		= 2,
	PWM_TIMER1_PRESCALER_4		= 3,
	PWM_TIMER1_PRESCALER_8		= 4,
	PWM_TIMER1_PRESCALER_16		= 5,
	PWM_TIMER1_PRESCALER_32		= 6,
	PWM_TIMER1_PRESCALER_64		= 7,
	PWM_TIMER1_PRESCALER_128	= 8,
	PWM_TIMER1_PRESCALER_256	= 9,
	PWM_TIMER1_PRESCALER_512	= 10,
	PWM_TIMER1_PRESCALER_1024	= 11,
	PWM_TIMER1_PRESCALER_2048	= 12,
	PWM_TIMER1_PRESCALER_4096	= 13,
	PWM_TIMER1_PRESCALER_8192	= 14,
	PWM_TIMER1_PRESCALER_16384	= 15
};

		void		pwm_timer1_init(uint8_t prescaler);
static	void		pwm_timer1_reset_counter(void);
static	uint16_t	pwm_timer1_get_counter(void);
static	void		pwm_timer1_set_max(uint16_t);
static	uint16_t	pwm_timer1_get_max(void);
		void		pwm_timer1_start(void);
		void		pwm_timer1_stop(void);
		void		pwm_timer1_set_oc1a(uint16_t value);
		uint16_t	pwm_timer1_get_oc1a(void);
		void		pwm_timer1_set_oc1b(uint16_t value);
		uint16_t	pwm_timer1_get_oc1b(void);
		void		pwm_timer1_set_oc1d(uint16_t value);
		uint16_t	pwm_timer1_get_oc1d(void);

static inline uint16_t pwm_timer1_get_counter(void)
{
	uint16_t rv;

	rv = TCNT1;
	rv |= TC1H << 8;

	return(rv);
}

static inline void pwm_timer1_reset_counter(void)
{
	TC1H	= 0;
	TCNT1	= 0;
}	

static inline void pwm_timer1_set_max(uint16_t max_value)
{
	TC1H	= (max_value & 0xff00) >> 8;
	OCR1C	= (max_value & 0x00ff) >> 0;
}

static inline uint16_t pwm_timer1_get_max(void)
{
	uint16_t rv;

	rv = OCR1C;
	rv |= TC1H << 8;

	return(rv);
}

#endif
