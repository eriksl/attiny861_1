#if !defined(_CLOCK_H_)
#define _CLOCK_H_

#include <stdint.h>

typedef enum
{
	CLOCK_SCALER_1		= 0,
	CLOCK_SCALER_2		= 1,
	CLOCK_SCALER_4		= 2,
	CLOCK_SCALER_8		= 3,
	CLOCK_SCALER_16		= 4,
	CLOCK_SCALER_32		= 5,
	CLOCK_SCALER_64		= 6,
	CLOCK_SCALER_128	= 7,
	CLOCK_SCALER_256	= 8
} clock_scaler_t;

void clock_set_scaler(uint8_t scaler);

#endif
