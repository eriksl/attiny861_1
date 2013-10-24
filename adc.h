#if !defined(_ADC_H_)
#define _ADC_H_
#include "ioports.h"

#include <stdint.h>
#include <avr/io.h>

void adc_init(void);
void adc_select(const adcport_t *source);
uint16_t adc_read(void);

#endif
