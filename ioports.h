#if !defined(_IOPORTS_H_)
#define _IOPORTS_H_ 1
#include <stdint.h>
#include <avr/io.h>

typedef struct
{
	uint8_t	refs[3];
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
} ioport_t;

typedef struct
{
	volatile	uint8_t		*port;
	volatile	uint8_t		*ddr;
	volatile	uint8_t		bit;
	volatile	uint8_t		*control_reg;
	volatile	uint8_t		com1_bit;
	volatile	uint8_t		com0_bit;
	volatile	uint8_t		foc_bit;
	volatile	uint8_t		pwm_bit;
	volatile	uint8_t		*compare_reg_high;
	volatile	uint8_t		*compare_reg_low;
} pwmport_t;

enum
{
	ADC_PORTS				= 4,
	INPUT_PORTS				= 4,
	OUTPUT_PORTS			= 1,
	INTERNAL_OUTPUT_PORTS	= 2,
	PWM_PORTS				= 3
};

extern const adcport_t	adc_ports[];
extern const ioport_t	input_ports[];
extern const ioport_t	output_ports[];
extern const ioport_t	internal_output_ports[];
extern const pwmport_t	pwm_ports[];

#endif
