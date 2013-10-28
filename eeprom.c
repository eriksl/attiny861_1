#include "eeprom.h"
#include "clock.h"

#include <avr/eeprom.h>

uint16_t eeprom_read_uint16(const uint16_t *from)
{
	uint16_t result;

	clock_set_scaler(CLOCK_SCALER_2);
	result = eeprom_read_word(from);
	clock_set_scaler(CLOCK_SCALER_1);
	return(result);
}

void eeprom_write_uint16(uint16_t *to, uint16_t value)
{
	clock_set_scaler(CLOCK_SCALER_2);
	eeprom_update_word(to, value);
	clock_set_scaler(CLOCK_SCALER_1);
}
