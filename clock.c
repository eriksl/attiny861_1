#include <avr/io.h>

#include "clock.h"

void clock_set_scaler(uint8_t scaler)
{
	CLKPR = _BV(CLKPCE);	// enable access
	CLKPR =	scaler & 0x0f;	// set scaler
}
