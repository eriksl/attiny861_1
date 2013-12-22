#if !defined(_IOPORTS_H_)
#define _IOPORTS_H_ 1
#include <stdint.h>
#include "avr.h"

typedef struct
{
	uint8_t	mux[6];
} adcport_t;

typedef struct
{
	volatile	uint8_t		*port;
	volatile	uint8_t		*pin;
	volatile	uint8_t		*ddr;
				uint8_t		bit;

	volatile	uint8_t		*pcmskreg;
				uint8_t		pcmskbit;
				uint8_t		gimskbit;

				uint8_t		state;
				uint32_t	counter;
} inport_t;

typedef struct
{
	volatile	uint8_t		*port;
	volatile	uint8_t		*ddr;
	volatile	uint8_t		bit;
} outport_t;

typedef struct
{
	volatile	uint8_t		*port;
	volatile	uint8_t		*ddr;
				uint8_t		bit;

				uint16_t	(*get)(void);
				void		(*set)(uint16_t);

				uint8_t		direction:1;
				uint16_t	current;
				uint16_t	limit_low;
				uint16_t	limit_high;
				uint16_t	step;
} pwmport_t;

#if (BOARD == 0)
#define	TEMP_PORTS				2
#define	ANALOG_PORTS			2
#define	INPUT_PORTS				4
#define	PWM_OUTPUT_PORTS		3
#define	PLAIN_OUTPUT_PORTS		1
#define	OUTPUT_PORTS			(PLAIN_OUTPUT_PORTS + PWM_OUTPUT_PORTS)
#define	USB_PORTS				0
#define	INTERNAL_OUTPUT_PORTS	2
#endif

#if (BOARD == 1)
#define	TEMP_PORTS				2
#define	ANALOG_PORTS			1
#define	INPUT_PORTS				2
#define	PWM_OUTPUT_PORTS		3
#define	PLAIN_OUTPUT_PORTS		1
#define	OUTPUT_PORTS			(PLAIN_OUTPUT_PORTS + PWM_OUTPUT_PORTS)
#define	USB_PORTS				0
#define	INTERNAL_OUTPUT_PORTS	2
#endif

extern adcport_t	temp_ports[];
extern adcport_t	analog_ports[];
extern inport_t		input_ports[];
extern pwmport_t	output_ports[];
extern outport_t	internal_output_ports[];
extern outport_t	usb_ports[];

#endif
