#if !defined(_TIMER0_H_)
#define _TIMER0_H_

#include <stdint.h>
#include "avr.h"

enum
{
	TIMER0_PRESCALER_OFF	= 0,
	TIMER0_PRESCALER_1		= 1,
	TIMER0_PRESCALER_8		= 2,
	TIMER0_PRESCALER_64		= 3,
	TIMER0_PRESCALER_256	= 4,
	TIMER0_PRESCALER_1024	= 5
};

		void		timer0_init(uint8_t scaler);
static	void		timer0_reset_counter(void);
static	uint8_t		timer0_get_counter(void);
		void		timer0_start(void);
		void		timer0_stop(void);
		uint8_t		timer0_status(void);
		uint16_t	timer0_get_compa(void);
		void		timer0_set_compa(uint16_t);
		uint16_t	timer0_get_compb(void);
		void		timer0_set_compb(uint16_t);

static inline void timer0_reset_counter(void)
{
	TCNT0L = 0;
}

static inline uint8_t timer0_get_counter(void)
{
	return(TCNT0L);
}

#endif
