#include "eeprom.h"
#include "clock.h"

#include <avr/eeprom.h>

uint16_t eeprom_read_uint16(const uint16_t *from)
{
	uint16_t result;

#if (USE_PLL == 1)
	clock_set_scaler(CLOCK_SCALER_2);
#endif
	result = eeprom_read_word(from);
#if (USE_PLL == 1)
	clock_set_scaler(CLOCK_SCALER_1);
#endif
	return(result);
}

void eeprom_write_uint16(uint16_t *to, uint16_t value)
{
#if (USE_PLL == 1)
	clock_set_scaler(CLOCK_SCALER_2);
#endif
	eeprom_update_word(to, value);
#if (USE_PLL == 1)
	clock_set_scaler(CLOCK_SCALER_1);
#endif
}
