#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <util/atomic.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "pwm_timer1.h"
#include "watchdog.h"

enum
{
	adc_warmup_init = 8
};

typedef enum
{
	pwm_mode_none				= 0,
	pwm_mode_fade_in			= 1,
	pwm_mode_fade_out			= 2,
	pwm_mode_fade_in_out_cont	= 3,
	pwm_mode_fade_out_in_cont	= 4
} pwm_mode_t;

typedef struct
{
	pwm_mode_t	pwm_mode:8;
} pwm_meta_t;

typedef struct
{
	uint32_t	counter;
	uint8_t		state;
} counter_meta_t;

static	pwm_meta_t		softpwm_meta[OUTPUT_PORTS];
static	pwm_meta_t		pwm_meta[PWM_PORTS];
static	counter_meta_t	counter_meta[INPUT_PORTS];

static	uint8_t		duty;
static	uint8_t		watchdog_counter;
static	uint8_t		i2c_sense_led, input_sense_led;

volatile static	const	uint8_t	*input_buffer;
volatile static			uint8_t	input_buffer_length;
volatile static			uint8_t	*output_buffer;
volatile static			uint8_t	*output_buffer_length;

static	uint8_t	input_byte;
static	uint8_t	input_command;
static	uint8_t	input_io;

static	uint8_t		adc_warmup;
static	uint16_t	adc_samples;
static	uint32_t	adc_value;

static void put_word(uint16_t from, uint8_t *to)
{
	to[0] = (from >> 8) & 0xff;
	to[1] = (from >> 0) & 0xff;
}

static void put_long(uint32_t from, uint8_t *to)
{
	to[0] = (from >> 24) & 0xff;
	to[1] = (from >> 16) & 0xff;
	to[2] = (from >>  8) & 0xff;
	to[3] = (from >>  0) & 0xff;
}

ISR(PCINT_vect)
{
	static uint8_t pc_dirty, pc_slot;

	if(watchdog_counter < 4)
		return;

	for(pc_slot = 0, pc_dirty = 0; pc_slot < INPUT_PORTS; pc_slot++)
	{
		if((*input_ports[pc_slot].pin & _BV(input_ports[pc_slot].bit)) ^ counter_meta[pc_slot].state)
		{
			counter_meta[pc_slot].counter++;
			pc_dirty = 1;
		}

		counter_meta[pc_slot].state = *input_ports[pc_slot].pin & _BV(input_ports[pc_slot].bit);
	}

	sei();

	if(pc_dirty)
	{
		*internal_output_ports[0].port |= _BV(internal_output_ports[0].bit);
		input_sense_led = 8;
	}
}

ISR(TIMER0_OVF_vect) // timer 0 softpwm overflow (default normal mode)
{
	if(timer0_get_compa() == 0)
		*output_ports[0].port &= ~_BV(output_ports[0].bit);
	else
		*output_ports[0].port |=  _BV(output_ports[0].bit);

	if(timer0_get_compb() == 0)
		*output_ports[1].port &= ~_BV(output_ports[1].bit);
	else
		*output_ports[1].port |=  _BV(output_ports[1].bit);
}

ISR(TIMER0_COMPA_vect) // timer 0 softpwm port 1 trigger
{
	if(timer0_get_compa() != 0xff)
		*output_ports[0].port &= ~_BV(output_ports[0].bit);
}

ISR(TIMER0_COMPB_vect) // timer 0 softpwm port 2 trigger
{
	if(timer0_get_compb() != 0xff)
		*output_ports[1].port &= ~_BV(output_ports[1].bit);
}

static inline void process_pwmmode(void)
{
	static uint8_t			pm_slot, pm_duty, pm_diff;
	static uint16_t			pm_duty16, pm_diff16;

	for(pm_slot = 0; pm_slot < OUTPUT_PORTS; pm_slot++)
	{
		if(pm_slot == 0)
			pm_duty = timer0_get_compa();
		else
			pm_duty = timer0_get_compb();

		pm_diff	= pm_duty / 10;

		if(pm_diff < 3)
			pm_diff = 3;

		switch(softpwm_meta[pm_slot].pwm_mode)
		{
			case(pwm_mode_none):
			{
				break;
			}
			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(pm_duty < (255 - pm_diff))
					pm_duty += pm_diff;
				else
				{
					pm_duty = 255;

					if(softpwm_meta[pm_slot].pwm_mode == pwm_mode_fade_in)
						softpwm_meta[pm_slot].pwm_mode = pwm_mode_none;
					else
						softpwm_meta[pm_slot].pwm_mode = pwm_mode_fade_out_in_cont;
				}

				if(pm_slot == 0)
					timer0_set_compa(pm_duty);
				else
					timer0_set_compb(pm_duty);

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(pm_duty > pm_diff)
					pm_duty -= pm_diff;
				else
				{
					pm_duty = 0;

					if(softpwm_meta[pm_slot].pwm_mode == pwm_mode_fade_out)
						softpwm_meta[pm_slot].pwm_mode = pwm_mode_none;
					else
						softpwm_meta[pm_slot].pwm_mode = pwm_mode_fade_in_out_cont;
				}

				if(pm_slot == 0)
					timer0_set_compa(pm_duty);
				else
					timer0_set_compb(pm_duty);

				break;
			}
		}
	}

	for(pm_slot = 0; pm_slot < PWM_PORTS; pm_slot++)
	{
		pm_duty16	= pwm_timer1_get_pwm(pm_slot);
		pm_diff16	= pm_duty16 / 8;

		if(pm_diff16 < 8)
			pm_diff16 = 8;

		switch(pwm_meta[pm_slot].pwm_mode)
		{
			case(pwm_mode_none):
			{
				break;
			}

			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(pm_duty16 < (1020 - pm_diff16))
					pm_duty16 += pm_diff16;
				else
				{
					pm_duty16 = 1020;

					if(pwm_meta[pm_slot].pwm_mode == pwm_mode_fade_in)
						pwm_meta[pm_slot].pwm_mode = pwm_mode_none;
					else
						pwm_meta[pm_slot].pwm_mode = pwm_mode_fade_out_in_cont;
				}

				pwm_timer1_set_pwm(pm_slot, pm_duty16);

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(pm_duty16 > pm_diff16)
					pm_duty16 -= pm_diff16;
				else
				{
					pm_duty16 = 0;

					if(pwm_meta[pm_slot].pwm_mode == pwm_mode_fade_out)
						pwm_meta[pm_slot].pwm_mode = pwm_mode_none;
					else
						pwm_meta[pm_slot].pwm_mode = pwm_mode_fade_in_out_cont;
				}

				pwm_timer1_set_pwm(pm_slot, pm_duty16);

				break;
			}
		}
	}
}

ISR(WDT_vect)
{
	if(watchdog_counter < 255)
		watchdog_counter++;

	sei();

	process_pwmmode();

	if(i2c_sense_led == 1)
		*internal_output_ports[1].port &= ~_BV(internal_output_ports[1].bit);

	if(i2c_sense_led > 0)
		i2c_sense_led--;

	if(input_sense_led == 1)
		*internal_output_ports[0].port &= ~_BV(internal_output_ports[0].bit);

	if(input_sense_led > 0)
		input_sense_led--;

	watchdog_setup(WATCHDOG_PRESCALER_2K);
}

static void reply(uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	if((reply_length + 4) > USI_TWI_BUFFER_SIZE)
		return;

	output_buffer[0] = 3 + reply_length;
	output_buffer[1] = error_code;
	output_buffer[2] = input_byte;

	for(ix = 0; ix < reply_length; ix++)
		output_buffer[3 + ix] = reply_string[ix];

	for(ix = 1, checksum = 0; ix < (3 + reply_length); ix++)
		checksum += output_buffer[ix];

	output_buffer[3 + reply_length] = checksum;
	*output_buffer_length = 3 + reply_length + 1;
}

static void reply_char(uint8_t value)
{
	reply(0, sizeof(value), &value);
}

static void reply_short(uint16_t value)
{
	uint8_t reply_string[sizeof(value)];

	put_word(value, reply_string);

	reply(0, sizeof(reply_string), reply_string);
}

static void reply_long(uint32_t value)
{
	uint8_t reply_string[sizeof(value)];

	put_long(value, reply_string);

	reply(0, sizeof(reply_string), reply_string);
}

static void reply_error(uint8_t error_code)
{
	reply(error_code, 0, 0);
}

static void extended_command()
{
	struct
	{
		uint8_t	amount;
		uint8_t	data[4];
	} control_info;

	switch(input_buffer[1])
	{
		case(0x00):	// get digital inputs
		{
			control_info.amount = INPUT_PORTS;
			put_long(0x3fffffff, &control_info.data[0]);
			return(reply(0, sizeof(control_info), (uint8_t *)&control_info));
		}

		case(0x01):	// get analog inputs
		{
			control_info.amount = ANALOG_PORTS;
			put_word(0x0000, &control_info.data[0]);
			put_word(0x03ff, &control_info.data[2]);
			return(reply(0, sizeof(control_info), (uint8_t *)&control_info));
		}

		case(0x02):	// get digital outputs
		{
			control_info.amount = OUTPUT_PORTS;
			put_word(0x0000, &control_info.data[0]);
			put_word(0x00ff, &control_info.data[2]);
			return(reply(0, sizeof(control_info), (uint8_t *)&control_info));
		}

		case(0x03):	// get pwm outputs
		{
			control_info.amount = PWM_PORTS;
			put_word(0x0000, &control_info.data[0]);
			put_word(0x03ff, &control_info.data[2]);
			return(reply(0, sizeof(control_info), (uint8_t *)&control_info));
		}

		case(0x04):	// get temperature sensors
		{
			control_info.amount = TEMP_PORTS;
			put_long(0x00, &control_info.data[0]);
			return(reply(0, sizeof(control_info), (uint8_t *)&control_info));
		}
	}

	return(reply_error(7));
}

static void process_command(volatile uint8_t twi_input_buffer_length, const volatile uint8_t *twi_input_buffer,
						uint8_t volatile *twi_output_buffer_length, volatile uint8_t *twi_output_buffer)
{
	*internal_output_ports[1].port |= _BV(internal_output_ports[1].bit);
	i2c_sense_led = 2;

	input_buffer_length		= twi_input_buffer_length;
	input_buffer			= twi_input_buffer;

	output_buffer_length	= twi_output_buffer_length;
	output_buffer			= twi_output_buffer;

	if(input_buffer_length < 1)
		return(reply_error(1));

	input_byte		= input_buffer[0];
	input_command	= input_byte & 0xf8;
	input_io		= input_byte & 0x07;

	switch(input_command)
	{
		case(0x00):	// short / no-io
		{
			switch(input_io)
			{
				case(0x00):	// identify
				{
					struct
					{
						uint8_t id1, id2;
						uint8_t model, version, revision;
						uint8_t name[16];
					} id =
					{
						0x4a, 0xfb,
						0x06, 0x01, 0x02,
						"attiny861a",
					};

					return(reply(0, sizeof(id), (uint8_t *)&id));
				}

				case(0x01):	// read analog input
				{
					uint8_t reply_string[6];

					put_word(adc_samples, &reply_string[0]);
					put_long(adc_value, &reply_string[2]);
					adc_warmup	= adc_warmup_init;
					adc_samples = 0;
					adc_value	= 0;

					return(reply(0, sizeof(reply_string), reply_string));
				}

				case(0x02):	// read temperature sensor
				{
					uint8_t reply_string[6];

					put_word(adc_samples, &reply_string[0]);
					put_long(adc_value, &reply_string[2]);
					adc_warmup	= adc_warmup_init;
					adc_samples = 0;
					adc_value	= 0;

					return(reply(0, sizeof(reply_string), reply_string));
				}

				case(0x07): // extended command
				{
					return(extended_command());
				}

				default:
				{
					return(reply_error(7));
				}
			}

			break;
		}

		case(0x10):	// 0x10 read counter
		case(0x20): // 0x20 read / reset counter
		{
			if(input_io >= INPUT_PORTS)
				return(reply_error(3));

			uint32_t counter = counter_meta[input_io].counter;

			if(input_command == 0x20)
				counter_meta[input_io].counter = 0;

			return(reply_long(counter));
		}

		case(0x30):	//	read input
		{
			uint8_t value;

			if(input_io >= INPUT_PORTS)
				return(reply_error(3));

			value = *input_ports[input_io].pin & _BV(input_ports[input_io].bit) ? 0x00 : 0x01;

			return(reply_char(value));
		}

		case(0x40):	//	write output / softpwm
		{
			uint8_t value;

			if(input_buffer_length < 2)
				return(reply_error(4));

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			value = input_buffer[1];

			if(input_io == 0)
				timer0_set_compa(value);
			else
				timer0_set_compb(value);

			return(reply_char(value));
		}

		case(0x50):	// read output / softpwm
		{
			uint8_t value;

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			if(input_io == 0)
				value = timer0_get_compa();
			else
				value = timer0_get_compb();

			return(reply_char(value));
		}

		case(0x60): // write softpwm mode
		{
			if(input_buffer_length < 2)
				return(reply_error(4));

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			if(input_buffer[1] > 3)
				return(reply_error(3));

			softpwm_meta[input_io].pwm_mode = input_buffer[1];

			return(reply_char(input_buffer[1]));
		}

		case(0x70):	// read softpwm mode
		{
			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			return(reply_char(softpwm_meta[input_io].pwm_mode));
		}

		case(0x80): // write pwm
		{
			uint16_t value;

			if(input_buffer_length < 3)
				return(reply_error(4));

			if(input_io >= PWM_PORTS)
				return(reply_error(3));

			value = input_buffer[1];
			value <<= 8;
			value |= input_buffer[2];

			pwm_timer1_set_pwm(input_io, value);

			return(reply_short(value));
		}

		case(0x90): // read pwm
		{
			if(input_io >= PWM_PORTS)
				return(reply_error(3));

			return(reply_short(pwm_timer1_get_pwm(input_io)));
		}

		case(0xa0): // write pwm mode
		{
			if(input_buffer_length < 2)
				return(reply_error(4));

			if(input_io >= PWM_PORTS)
				return(reply_error(3));

			if(input_buffer[1] > 3)
				return(reply_error(3));

			pwm_meta[input_io].pwm_mode = input_buffer[1];

			return(reply_char(input_buffer[1]));
		}

		case(0xb0):	// read pwm mode
		{
			if(input_io >= PWM_PORTS)
				return(reply_error(3));

			return(reply_char(pwm_meta[input_io].pwm_mode));
		}

		case(0xc0):	// select adc
		{
			if(input_io >= ANALOG_PORTS)
				return(reply_error(3));

			adc_select(&analog_ports[input_io]);
			adc_warmup	= adc_warmup_init;
			adc_samples = 0;
			adc_value	= 0;

			return(reply_char(input_io));
		}

		case(0xd0):	// select temperature sensor
		{
			if(input_io >= TEMP_PORTS)
				return(reply_error(3));

			adc_select(&temp_ports[input_io]);
			adc_warmup	= adc_warmup_init;
			adc_samples = 0;
			adc_value	= 0;

			return(reply_char(input_io));
		}

		case(0xe0):	// calibrate temperature sensor // FIXME
		{
			if(input_io >= TEMP_PORTS)
				return(reply_error(3));

			return(reply_char(input_io));
		}

		default:
		{
			return(reply_error(2));
		}
	}

	return(reply_error(2));
}

void twi_idle(void)
{
	if(adc_warmup > 0)
		adc_warmup--;
	else
	{
		//if(adc_samples < 256)
		//{
			adc_samples++;
			adc_value += adc_read();
		//}
	}
}

int main(void)
{
	uint8_t slot;

	cli();

	PRR =		(0 << 7)		|
				(0 << 6)		|	// reserved
				(0 << 5)		|
				(0 << 4)		|
				(0 << PRTIM1)	|	// timer1
				(0 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(0 << PRADC);		// adc / analog comperator

	DDRA	= 0;
	DDRB	= 0;
	GIMSK	= 0;
	PCMSK0	= 0;
	PCMSK1	= 0;

	for(slot = 0; slot < INPUT_PORTS; slot++)
	{
		*input_ports[slot].ddr			|=  _BV(input_ports[slot].bit);	// output
		*input_ports[slot].port			&= ~_BV(input_ports[slot].bit);	// low
		*input_ports[slot].ddr			&= ~_BV(input_ports[slot].bit);	// input
		*input_ports[slot].port			&= ~_BV(input_ports[slot].bit); // disable pullup
		*input_ports[slot].pcmskreg		|=  _BV(input_ports[slot].pcmskbit);
		GIMSK							|=  _BV(input_ports[slot].gimskbit);

		counter_meta[slot].state		= *input_ports[slot].pin & _BV(input_ports[slot].bit);
		counter_meta[slot].counter		= 0;
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		*output_ports[slot].ddr		|=  _BV(output_ports[slot].bit);
		*output_ports[slot].port	&= ~_BV(output_ports[slot].bit);
		softpwm_meta[slot].pwm_mode	= pwm_mode_none;
	}

	for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
	{
		*internal_output_ports[slot].ddr	|= _BV(internal_output_ports[slot].bit);
		*internal_output_ports[slot].port	&= ~_BV(internal_output_ports[slot].bit);
	}

	for(slot = 0; slot < PWM_PORTS; slot++)
	{
		*pwm_ports[slot].ddr 		|=  _BV(pwm_ports[slot].bit);
		*pwm_ports[slot].port		&= ~_BV(pwm_ports[slot].bit);
		pwm_meta[slot].pwm_mode		= pwm_mode_none;
	}

	adc_init();

	// 8 mhz / 256 / 256 = 122 Hz
	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_compa(0x00);
	timer0_set_compb(0x00);
	timer0_start();

	// 8 mhz / 32 / 1024 = 244 Hz
	pwm_timer1_init(PWM_TIMER1_PRESCALER_32);
	pwm_timer1_set_max(0x3ff);
	pwm_timer1_start();

	for(duty = 0; duty < 3; duty++)
	{
		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			*internal_output_ports[slot].port |= _BV(internal_output_ports[slot].bit);
			_delay_ms(25);
		}

		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			*internal_output_ports[slot].port &= ~_BV(internal_output_ports[slot].bit);
			_delay_ms(25);
		}

		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			*internal_output_ports[slot].port |= _BV(internal_output_ports[slot].bit);
			_delay_ms(25);
		}

		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			*internal_output_ports[slot].port &= ~_BV(internal_output_ports[slot].bit);
			_delay_ms(25);
		}
	}

	watchdog_setup(WATCHDOG_PRESCALER_2K);
	watchdog_start();

	adc_warmup	= adc_warmup_init;
	adc_samples	= 0;
	adc_value	= 0;

	usi_twi_slave(0x02, 0, process_command, twi_idle);
}
