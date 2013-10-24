#if !defined(_ADC_H_)
#define _ADC_H_
#include "ioports.h"

#include <stdint.h>
#include <avr/io.h>

void adc_init(void);
void adc_select(const adcport_t *source);

static inline void adc_start(void)
{
	ADCSRA |= _BV(ADSC);
}

static inline uint8_t adc_ready(void)
{
	return(!(ADCSRA & _BV(ADSC)));
}

static inline uint16_t adc_read(void)
{
	return(ADCW);
}

#endif
