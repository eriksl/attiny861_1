#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "pwm_timer1.h"
#include "watchdog.h"

enum
{
	WATCHDOG_PRESCALER = WATCHDOG_PRESCALER_2K,
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
	uint8_t		duty;
	pwm_mode_t	pwm_mode:8;
} pwm_meta_t;

typedef struct
{
	uint32_t	counter;
	uint8_t		state;
} counter_meta_t;

static	const	ioport_t		*ioport;
static			pwm_meta_t		softpwm_meta[OUTPUT_PORTS];
static			pwm_meta_t		pwm_meta[PWM_PORTS];
static			pwm_meta_t		*pwm_slot;
static			counter_meta_t	counter_meta[INPUT_PORTS];
static			counter_meta_t	*counter_slot;

static	uint8_t		watchdog_counter;
static	uint8_t		slot, dirty, duty, next_duty, diff;
static	uint8_t		timer0_value, timer0_debug_1, timer0_debug_2;
static	uint8_t		i2c_sense_led, input_sense_led;
static	uint16_t	duty16, diff16;

static	void update_static_softpwm_ports(void);

static void put_word(uint16_t from, uint8_t *to)
{
	to += 2;

	*(--to) = from & 0xff;
	from >>= 8;
	*(--to) = from & 0xff;
}

static void put_long(uint32_t from, uint8_t *to)
{
	to += 4;

	*(--to) = from & 0xff;
	from >>= 8;
	*(--to) = from & 0xff;
	from >>= 8;
	*(--to) = from & 0xff;
	from >>= 8;
	*(--to) = from & 0xff;
}

ISR(WDT_vect)
{
	dirty = 0;

	if(watchdog_counter < 255)
		watchdog_counter++;

	pwm_slot = &softpwm_meta[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		duty	= pwm_slot->duty;
		diff	= duty / 10;

		if(diff < 3)
			diff = 3;

		switch(pwm_slot->pwm_mode)
		{
			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(duty < (255 - diff))
					duty += diff;
				else
				{
					duty = 255;

					if(pwm_slot->pwm_mode == pwm_mode_fade_in)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_out_in_cont;
				}

				pwm_slot->duty = duty;

				dirty = 1;

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(duty > diff)
					duty -= diff;
				else
				{
					duty = 0;

					if(pwm_slot->pwm_mode == pwm_mode_fade_out)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_in_out_cont;
				}

				pwm_slot->duty = duty;

				dirty = 1;

				break;
			}
		}

		pwm_slot++;
	}

	if(dirty)
	{
		update_static_softpwm_ports();
		timer0_start();
	}

	pwm_slot = &pwm_meta[0];

	for(slot = 0; slot < PWM_PORTS; slot++)
	{
		duty16	= pwm_timer1_get_pwm(slot);
		diff16	= duty16 / 8;

		if(diff16 < 8)
			diff16 = 8;

		switch(pwm_slot->pwm_mode)
		{
			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(duty16 < (1020 - diff16))
					duty16 += diff16;
				else
				{
					duty16 = 1020;

					if(pwm_slot->pwm_mode == pwm_mode_fade_in)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_out_in_cont;
				}

				pwm_timer1_set_pwm(slot, duty16);

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(duty16 > diff16)
					duty16 -= diff16;
				else
				{
					duty16 = 0;

					if(pwm_slot->pwm_mode == pwm_mode_fade_out)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_in_out_cont;
				}

				pwm_timer1_set_pwm(slot, duty16);

				break;
			}
		}

		pwm_slot++;
	}

	if(i2c_sense_led == 1)
		*internal_output_ports[0].port &= ~_BV(internal_output_ports[0].bit);

	if(i2c_sense_led > 0)
		i2c_sense_led--;

	if(input_sense_led == 1)
		*internal_output_ports[1].port &= ~_BV(internal_output_ports[1].bit);

	if(input_sense_led > 0)
		input_sense_led--;

	watchdog_setup(WATCHDOG_PRESCALER);
}

ISR(TIMER0_COMPA_vect) // timer 0 softpwm overflow
{
	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty == 0)				// pwm duty == 0, port is off, set it off
			*ioport->port &= ~_BV(ioport->bit);
		else									// else set the port on
			*ioport->port |=  _BV(ioport->bit);

		pwm_slot++;
		ioport++;
	}
}

ISR(TIMER0_COMPB_vect) // timer 0 softpwm trigger
{
	timer0_value = timer0_get_counter();

	if(timer0_value < 253)
		timer0_value += 1;
	else
		timer0_value = 255;

	next_duty = 255;

	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty <= timer0_value)
			*ioport->port &= ~_BV(ioport->bit);
		else
			if(pwm_slot->duty < next_duty)
				next_duty = pwm_slot->duty;

		pwm_slot++;
		ioport++;
	}

	if(next_duty == 255)
	{
		next_duty = 255;

		pwm_slot = &softpwm_meta[0];

		for(slot = OUTPUT_PORTS; slot > 0; slot--)
		{
			if((pwm_slot->duty != 0) && (pwm_slot->duty < next_duty))
				next_duty = pwm_slot->duty;

			pwm_slot++;
		}

		if(next_duty == 255)
			timer0_stop();
	}

	timer0_set_trigger(next_duty);
}

ISR(PCINT_vect)
{
	dirty = 0;

	for(slot = 0; slot < INPUT_PORTS; slot++)
	{
		ioport			= &input_ports[slot];
		counter_slot	= &counter_meta[slot];

		if((watchdog_counter > 4) && ((*ioport->pin & _BV(ioport->bit)) ^ counter_slot->state))
		{
			counter_slot->counter++;
			dirty = 1;
		}

		counter_slot->state = *ioport->pin & _BV(ioport->bit);
	}

	if(dirty)
	{
		*internal_output_ports[1].port |= _BV(internal_output_ports[1].bit);
		input_sense_led = 8;
	}
}

static void update_static_softpwm_ports(void)
{
	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty == 0)
			*ioport->port &= ~_BV(ioport->bit);
		else if(pwm_slot->duty == 255)
			*ioport->port |= _BV(ioport->bit);

		pwm_slot++;
		ioport++;
	}
}

static void build_reply(uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer,
		uint8_t command, uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	output_buffer[0] = 3 + reply_length;
	output_buffer[1] = error_code;
	output_buffer[2] = command;

	for(ix = 0; ix < reply_length; ix++)
		output_buffer[3 + ix] = reply_string[ix];

	for(ix = 1, checksum = 0; ix < (3 + reply_length); ix++)
		checksum += output_buffer[ix];

	output_buffer[3 + reply_length] = checksum;
	*output_buffer_length = 3 + reply_length + 1;
}

static void extended_command(uint8_t buffer_size, volatile uint8_t input_buffer_length, const volatile uint8_t *input_buffer,
						uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer)
{
	uint8_t command = input_buffer[1];

	if(command < 5)
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
				break;
			}

			case(0x01):	// get analog inputs
			{
				control_info.amount = ADC_PORTS;
				put_word(0x0000, &control_info.data[0]);
				put_word(0x03ff, &control_info.data[2]);
				break;
			}

			case(0x02):	// get digital outputs
			{
				control_info.amount = OUTPUT_PORTS;
				put_word(0x0000, &control_info.data[0]);
				put_word(0x00ff, &control_info.data[2]);
				break;
			}

			case(0x03):	// get pwm outputs
			{
				control_info.amount = PWM_PORTS;
				put_word(0x0000, &control_info.data[0]);
				put_word(0x03ff, &control_info.data[2]);
				break;
			}

			default:
			{
				return(build_reply(output_buffer_length, output_buffer, input_buffer[0], 7, 0, 0));
			}
		}

		return(build_reply(output_buffer_length, output_buffer, input_buffer[0], 0, sizeof(control_info), (uint8_t *)&control_info));
	}

	return(build_reply(output_buffer_length, output_buffer, input_buffer[0], 7, 0, 0));
}

static void twi_callback(uint8_t buffer_size, volatile uint8_t input_buffer_length, const volatile uint8_t *input_buffer,
						uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer)
{
	uint8_t input;
	uint8_t	command;
	uint8_t	io;

	*internal_output_ports[0].port |= _BV(internal_output_ports[0].bit);
	i2c_sense_led = 2;

	if(input_buffer_length < 1)
		return(build_reply(output_buffer_length, output_buffer, 0, 1, 0, 0));

	input	= input_buffer[0];
	command	= input & 0xf8;
	io		= input & 0x07;

	switch(command)
	{
		case(0x00):	// short / no-io
		{
			switch(io)
			{
				case(0x00):	// identify
				{
					struct
					{
						uint8_t id1, id2;
						uint8_t model, version, revision;
						uint8_t name[16];
					} reply =
					{
						0x4a, 0xfb,
						0x06, 0x01, 0x01,
						"attiny861a",
					};

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(reply), (uint8_t *)&reply));
				}

				case(0x01):	// 0x02 read ADC
				{
					uint16_t value;

					value = ADCW;

					if(ADCSRA & _BV(ADSC))	// conversion not ready
						return(build_reply(output_buffer_length, output_buffer, input, 5, 0, 0));

					adc_stop();

					uint8_t replystring[2];

					put_word(value, replystring);

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x02): // 0x02 DEBUG read timer0 counter
				{
					uint8_t value = timer0_get_counter();
					return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
				}

				case(0x03): // 0x03 DEBUG read timer1 counter
				{
					uint16_t value = pwm_timer1_get_counter();
					uint8_t replystring[2];

					put_word(value, replystring);

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x04): // 0x04 DEBUG read timer1 max
				{
					uint16_t value = pwm_timer1_get_max();
					uint8_t replystring[2];

					put_word(value, replystring);

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x05): // 0x05 read timer1 prescaler
				{
					uint8_t value = pwm_timer1_status();

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(value), &value));
				}

				case(0x06): // 0x06 DEBUG read timer0 entry / exit counter values
				{
					uint8_t value[2];

					value[0] = timer0_debug_1;
					value[1] = timer0_debug_2;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(value), value));
				}

				case(0x07): // extended command
				{
					return(extended_command(buffer_size, input_buffer_length, input_buffer, output_buffer_length, output_buffer));
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 7, 0, 0));
				}
			}

			break;
		}

		case(0x10):	// 0x10 read counter
		case(0x20): // 0x20 read / reset counter
		{
			if(io >= INPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t		replystring[4];
			uint32_t	counter = counter_meta[io].counter;

			if(command == 0x20)
				counter_meta[io].counter = 0;

			put_long(counter, replystring);

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		case(0x30):	//	read input
		{
			uint8_t value;

			if(io >= INPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			ioport	= &input_ports[io];
			value	= *ioport->pin & _BV(ioport->bit) ? 0xff : 0x00;

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
		}

		case(0x40):	//	write output / softpwm
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			softpwm_meta[io].duty = input_buffer[1];
			update_static_softpwm_ports();
			timer0_start();
			duty = softpwm_meta[io].duty;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(duty), &duty));
		}

		case(0x50):	// read output / softpwm
		{
			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			duty = softpwm_meta[io].duty;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(duty), &duty));
		}

		case(0x60): // write softpwm mode
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode = input_buffer[1];

			if(mode > 3)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			softpwm_meta[io].pwm_mode = mode;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(mode), &mode));
		}

		case(0x70):	// read softpwm mode
		{
			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode;

			mode = softpwm_meta[io].pwm_mode;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(mode), &mode));
		}

		case(0x80): // write pwm
		{
			if(input_buffer_length < 3)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint16_t value;

			value = input_buffer[1];
			value <<= 8;
			value |= input_buffer[2];

			pwm_timer1_set_pwm(io, value);

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0x90): // read pwm
		{
			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint16_t value = pwm_timer1_get_pwm(io);
			uint8_t reply[2];

			put_word(value, reply);

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(reply), reply));
		}

		case(0xa0): // write pwm mode
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode = input_buffer[1];

			if(mode > 3)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			pwm_meta[io].pwm_mode = mode;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(mode), &mode));
		}

		case(0xb0):	// read pwm mode
		{
			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode;

			mode = pwm_meta[io].pwm_mode;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(mode), &mode));
		}

		case(0xc0):	// start adc conversion
		{
			if(io >= ADC_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			adc_start(io);
			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xf0):	// twi stats
		{
			uint8_t		replystring[2];
			uint16_t	stats;

			switch(io)
			{
				case(0x00):	//	disable
				{
					usi_twi_enable_stats(0);
					return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
				}

				case(0x01):	//	enable
				{
					usi_twi_enable_stats(1);
					return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
				}

				case(0x02):	//	read start conditions
				{
					stats = usi_twi_stats_start_conditions();
					break;
				}

				case(0x03):	//	read stop conditions
				{
					stats = usi_twi_stats_stop_conditions();
					break;
				}

				case(0x04):	//	read error conditions
				{
					stats = usi_twi_stats_error_conditions();
					break;
				}

				case(0x05):	//	read overflow conditions
				{
					stats = usi_twi_stats_overflow_conditions();
					break;
				}

				case(0x06):	//	read local frames
				{
					stats = usi_twi_stats_local_frames();
					break;
				}

				case(0x07):	//	read idle calls
				{
					stats = usi_twi_stats_idle_calls();
					break;
				}
			}

			put_word(stats, replystring);

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}
		default:
		{
			return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
		}
	}

	return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
}

int main(void)
{
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
		*ioport->port		&= ~_BV(ioport->bit);
		*ioport->ddr		&= ~_BV(ioport->bit);
		*ioport->port		|=  _BV(ioport->bit);
		*ioport->pcmskreg	|=  _BV(ioport->pcmskbit);
		GIMSK				|=  _BV(ioport->gimskbit);

		ioport					= &input_ports[slot];
		counter_slot			= &counter_meta[slot];
		counter_slot->state		= *ioport->pin & _BV(ioport->bit);
		counter_slot->counter	= 0;
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		ioport = &output_ports[slot];

		*ioport->ddr	|= _BV(ioport->bit);
		*ioport->port	&= ~_BV(ioport->bit);
		softpwm_meta[slot].duty		= 0;
		softpwm_meta[slot].pwm_mode	= pwm_mode_none;
	}

	for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
	{
		ioport = &internal_output_ports[slot];

		*ioport->ddr	|= _BV(ioport->bit);
		*ioport->port	&= ~_BV(ioport->bit);
	}

	for(slot = 0; slot < PWM_PORTS; slot++)
	{
		*pwm_ports[slot].ddr 		|= _BV(pwm_ports[slot].bit);
		*pwm_ports[slot].port		&= ~_BV(pwm_ports[slot].bit);
		pwm_meta[slot].pwm_mode		= pwm_mode_none;
	}

	for(duty = 0; duty < 3; duty++)
	{
		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			ioport = &internal_output_ports[slot];
			*ioport->port |= _BV(ioport->bit);
		}

		for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
		{
			ioport = &internal_output_ports[slot];
			*ioport->port &= ~_BV(ioport->bit);
		}

		for(slot = INTERNAL_OUTPUT_PORTS; slot > 0; slot--)
		{
			ioport = &internal_output_ports[slot - 1];
			*ioport->port |= _BV(ioport->bit);
		}

		for(slot = INTERNAL_OUTPUT_PORTS; slot > 0; slot--)
		{
			ioport = &internal_output_ports[slot - 1];
			*ioport->port &= ~_BV(ioport->bit);
		}
	}

	adc_init();

	// 8 mhz / 256 / 256 = 122 Hz
	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_max(0xff);

	// 8 mhz / 32 / 1024 = 244 Hz
	pwm_timer1_init(PWM_TIMER1_PRESCALER_32);
	pwm_timer1_set_max(0x3ff);
	pwm_timer1_start();

	watchdog_setup(WATCHDOG_PRESCALER);
	watchdog_start();

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
