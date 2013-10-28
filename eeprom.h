#if !defined(_EEPROM_H_)
#define _EEPROM_H_

#include <stdint.h>

uint16_t	eeprom_read_uint16(const uint16_t *);
void		eeprom_write_uint16(uint16_t *, uint16_t value);

#endif
