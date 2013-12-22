#include <stdint.h>

#include <avr/interrupt.h>
#include <util/delay.h>

#include <usitwislave.h>

#include "avr.h"
#include "clock.h"
#include "eeprom.h"
#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "pwm_timer1.h"
#include "watchdog.h"

enum
{
	adc_warmup_init = 8,
	command_led_timeout = 8,
	if_active_led_timeout = 2,
	phy_buffer_size = USI_TWI_BUFFER_SIZE
};

static	uint8_t	const *phy_receive_buffer;
static	uint8_t	phy_receive_buffer_length;
static	uint8_t	*phy_send_buffer;
static	uint8_t	*phy_send_buffer_length;
static	uint8_t	duty;
static	uint8_t	init_counter = 0;
static	uint8_t	if_sense_led, input_sense_led;

static	uint8_t	input_byte;
static	uint8_t	input_command;
static	uint8_t	input_io;

static	int16_t	temp_cal_multiplier;
static	int16_t	temp_cal_offset;

static	uint8_t		adc_warmup;
static	uint16_t	adc_samples;
static	uint32_t	adc_value;

typedef struct 
{
	uint16_t	multiplier;
	uint16_t	offset;
} eeprom_temp_cal_t;

typedef struct
{
	eeprom_temp_cal_t temp_cal[8];
} eeprom_t;

static eeprom_t *eeprom = (eeprom_t *)0;

static uint16_t get_word(const uint8_t *from)
{
	uint16_t result;

	result = from[0];
	result <<= 8;
	result |= from[1];

	return(result);
}

#if 0
static uint32_t get_long(const uint8_t *from)
{
	uint32_t result;

	result = from[0];
	result <<= 8;
	result |= from[1];
	result <<= 8;
	result |= from[2];
	result <<= 8;
	result |= from[3];

	return(result);
}
#endif

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

static void process_fading(void)
{
	static uint8_t		output_index;
	static pwmport_t*	output_slot;
	static uint16_t		current, low, high, step;

	for(output_index = 0; output_index < OUTPUT_PORTS; output_index++)
	{
		output_slot = &output_ports[output_index];

		if((step = output_slot->step) == 0)
			continue;

		if(!output_slot->set)
			continue;

		current	= output_slot->current;
		low		= output_slot->limit_low;
		high	= output_slot->limit_high;

		if(output_slot->direction)											// up
		{
			if(((current + step) < current) || ((current + step) > high))	// wrap or threshold
			{
				current = high;
				output_slot->direction = 0;									// down
			}
			else
				current += step;
		} 
		else																// down
		{
			if(((current - step) > current) || ((current - step) < low))	// wrap or threshold
			{
				current = low;
				output_slot->direction = 1;									//up
			}
			else
				current -= step;
		}

		output_slot->current = current;
		output_slot->set(current);
	}
}

ISR(PCINT_vect, ISR_NOBLOCK)
{
	static uint8_t	pc_dirty, pc_index;
	static inport_t *input_slot;

	if(init_counter < 16)	// discard spurious first few interrupts
		return;

	for(pc_index = 0, pc_dirty = 0; pc_index < INPUT_PORTS; pc_index++)
	{
		input_slot = &input_ports[pc_index];

		if((*input_slot->pin & _BV(input_slot->bit)) ^ input_slot->state)
		{
			input_slot->counter++;
			pc_dirty = 1;
		}

		input_slot->state = *input_slot->pin & _BV(input_slot->bit);
	}

	if(pc_dirty)
	{
		*internal_output_ports[0].port |= _BV(internal_output_ports[0].bit);
		input_sense_led = command_led_timeout;
	}
}

ISR(TIMER0_OVF_vect, ISR_NOBLOCK) // timer 0 softpwm overflow (default normal mode) (244 Hz)
{
	static uint8_t pwm_divisor = 0;

#if (PLAIN_OUTPUT_PORTS > 0)
	if(output_ports[PWM_OUTPUT_PORTS + 0].current == 0)
		*output_ports[PWM_OUTPUT_PORTS + 0].port &= ~_BV(output_ports[PWM_OUTPUT_PORTS + 0].bit);
	else
		*output_ports[PWM_OUTPUT_PORTS + 0].port |=  _BV(output_ports[PWM_OUTPUT_PORTS + 0].bit);
#endif

#if (PLAIN_OUTPUT_PORTS > 1)
	if(output_ports[PWM_OUTPUT_PORTS + 1].current == 0)
		*output_ports[PWM_OUTPUT_PORTS + 1].port &= ~_BV(output_ports[PWM_OUTPUT_PORTS + 1].bit);
	else
		*output_ports[PWM_OUTPUT_PORTS + 1].port |=  _BV(output_ports[PWM_OUTPUT_PORTS + 1].bit);
#endif

	if(init_counter < 255)
		init_counter++;

	if(++pwm_divisor > 4)
	{
		process_fading();
		pwm_divisor = 0;
	}

	if(if_sense_led == 1)
		*internal_output_ports[1].port &= ~_BV(internal_output_ports[1].bit);

	if(if_sense_led > 0)
		if_sense_led--;

	if(input_sense_led == 1)
		*internal_output_ports[0].port &= ~_BV(internal_output_ports[0].bit);

	if(input_sense_led > 0)
		input_sense_led--;
}

ISR(TIMER0_COMPA_vect, ISR_NOBLOCK) // timer 0 softpwm port 1 trigger
{
	if(timer0_get_compa() != 0xff)
		*output_ports[PWM_OUTPUT_PORTS + 0].port &= ~_BV(output_ports[PWM_OUTPUT_PORTS + 0].bit);
}

ISR(TIMER0_COMPB_vect, ISR_NOBLOCK) // timer 0 softpwm port 2 trigger
{
	if(timer0_get_compb() != 0xff)
		*output_ports[PWM_OUTPUT_PORTS + 1].port &= ~_BV(output_ports[PWM_OUTPUT_PORTS + 1].bit);
}

static void reply(uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	if((reply_length + 4) > phy_buffer_size)
		return;

	phy_send_buffer[0] = 3 + reply_length;
	phy_send_buffer[1] = error_code;
	phy_send_buffer[2] = input_byte;

	for(ix = 0; ix < reply_length; ix++)
		phy_send_buffer[3 + ix] = reply_string[ix];

	for(ix = 1, checksum = 0; ix < (3 + reply_length); ix++)
		checksum += phy_send_buffer[ix];

	phy_send_buffer[3 + reply_length] = checksum;
	*phy_send_buffer_length = 3 + reply_length + 1;
}

static void reply_char(uint8_t value)
{
	return(reply(0, sizeof(value), &value));
}

static void reply_short(uint16_t value)
{
	uint8_t reply_string[sizeof(value)];

	put_word(value, reply_string);

	return(reply(0, sizeof(reply_string), reply_string));
}

static void reply_long(uint32_t value)
{
	uint8_t reply_string[sizeof(value)];

	put_long(value, reply_string);

	return(reply(0, sizeof(reply_string), reply_string));
}

static void reply_error(uint8_t error_code)
{
	return(reply(error_code, 0, 0));
}

static void reply_ok(void)
{
	return(reply(0, 0, 0));
}

static void process_input(uint8_t if_input_buffer_length, const uint8_t *if_input_buffer,
						uint8_t *if_output_buffer_length, uint8_t *if_output_buffer)
{
	*internal_output_ports[1].port |= _BV(internal_output_ports[1].bit);
	if_sense_led = command_led_timeout;

	phy_receive_buffer_length	= if_input_buffer_length;
	phy_receive_buffer			= if_input_buffer;

	phy_send_buffer_length	= if_output_buffer_length;
	phy_send_buffer			= if_output_buffer;

	if(phy_receive_buffer_length < 1)
		return(reply_error(1));

	input_byte		= phy_receive_buffer[0];
	input_command	= input_byte & 0xf8;
	input_io		= input_byte & 0x07;

	watchdog_reset();

	switch(input_command)
	{
		case(0x00):	// short / no-io
		{
			switch(input_io)
			{
				case(0x00):	// identify
				{
					static const struct
					{
						uint8_t id1, id2;
						uint8_t model, version, revision;
						uint8_t digital_inputs, analog_inputs, temperature_sensors, digital_outputs;
						uint8_t name[12];
					} id =
					{
						0x4a, 0xfb,
						0x06, 0x01, 0x06,
						INPUT_PORTS, ANALOG_PORTS, TEMP_PORTS, OUTPUT_PORTS,
						"attiny861a",
					};

					return(reply(0, sizeof(id), (uint8_t *)&id));
				}

				case(0x01):	// read analog input
				{
					uint8_t reply_string[6];
					uint32_t scaled_value = adc_value << 6; // scale 10 bits to 16 bits

					put_word(adc_samples, &reply_string[0]);
					put_long(scaled_value, &reply_string[2]);
					adc_warmup	= adc_warmup_init;
					adc_samples = 0;
					adc_value	= 0;

					return(reply(0, sizeof(reply_string), reply_string));
				}

				case(0x02):	// read temperature sensor
				{
					int32_t	value;
					int32_t	samples;

					uint8_t reply_string[8];

					value	= (int32_t)adc_value;
					samples	= (int32_t)adc_samples;

					value	*= 10;
					value	*= temp_cal_multiplier;
					value	/= samples;
					value	/= 1000;
					value	+= temp_cal_offset;

					put_word(value, &reply_string[0]);
					put_word(adc_samples, &reply_string[2]);
					put_long(adc_value, &reply_string[4]);

					adc_warmup	= adc_warmup_init;
					adc_samples = 0;
					adc_value	= 0;

					return(reply(0, sizeof(reply_string), reply_string));
				}

				case(0x06): // test watchdog
				{
					for(;;)
						(void)0;
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

			uint32_t counter = input_ports[input_io].counter;

			if(input_command == 0x20)
				input_ports[input_io].counter = 0;

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

		case(0x40):	// write output static value=uint16/scaled
		{
			uint16_t	value;
			pwmport_t	*port;

			if(phy_receive_buffer_length != 3)
				return(reply_error(4));

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			port = &output_ports[input_io];

			port->step = 0;
			value = get_word(&phy_receive_buffer[1]);
			port->current = port->limit_high = port->limit_low = value; // disable stepping

			if(port->set) // pwm possible
			{
				port->set(value);
				if(value == 0)
					*port->port &= ~_BV(port->bit);
				else
					if(value == 0xffff)
						*port->port |=  _BV(port->bit);
			}
			else
			{
				if(value)
					*port->port |=  _BV(port->bit);
				else
					*port->port &= ~_BV(port->bit);
			}

			return(reply_ok());
		}

		case(0x50):	// read	output static value=uint16/scaled
		{
			uint16_t value;

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			if(output_ports[input_io].get) // pwm possible
				value = output_ports[input_io].get();
			else
			{
				if(*output_ports[input_io].port & _BV(output_ports[input_io].bit))
					value = 0xffff;
				else
					value = 0;
			}

			return(reply_short(value));
		}

		case(0x60): // write output	stepping min_value=uint16/scaled, max_value=uint16/scaled, step=uint16/scaled
		{
			uint16_t low;
			pwmport_t *port;

			if(phy_receive_buffer_length != 7)
				return(reply_error(4));

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			if(!output_ports[input_io].get)
				return(reply_error(7));

			port = &output_ports[input_io];

			low = get_word(&phy_receive_buffer[1]);

			port->limit_low		= low;
			port->limit_high	= get_word(&phy_receive_buffer[3]);
			port->current		= low;
			port->step			= get_word(&phy_receive_buffer[5]);

			return(reply_ok());
		}

		case(0x70):	// read output stepping min_value=uint16/scaled, max_value=uint16/scaled, step=uint16/scaled, current uint16/scaled
		{
			uint8_t rv[8];
			pwmport_t *port;

			if(input_io >= OUTPUT_PORTS)
				return(reply_error(3));

			port = &output_ports[input_io];

			put_word(port->limit_low, &rv[0]);
			put_word(port->limit_high, &rv[2]);
			put_word(port->step, &rv[4]);
			put_word(port->current, &rv[6]);

			return(reply(0, sizeof(rv), rv));
		}

		case(0xc0):	// select adc
		{
			if(input_io >= ANALOG_PORTS)
				return(reply_error(3));

			adc_select(&analog_ports[input_io]);
			adc_warmup	= adc_warmup_init;
			adc_samples = 0;
			adc_value	= 0;

			return(reply_ok());
		}

		case(0xd0):	// select temperature sensor
		{
			if(input_io >= TEMP_PORTS)
				return(reply_error(3));

			adc_select(&temp_ports[input_io]);
			adc_warmup	= adc_warmup_init;
			adc_samples = 0;
			adc_value	= 0;

			temp_cal_multiplier = eeprom_read_uint16(&eeprom->temp_cal[input_io].multiplier);
			temp_cal_offset		= eeprom_read_uint16(&eeprom->temp_cal[input_io].offset);

			return(reply_ok());
		}

		case(0xe0):	// set temperature sensor calibration values
		{
			int32_t value;

			if(input_io >= TEMP_PORTS)
				return(reply_error(3));

			if(phy_receive_buffer_length < 5)
				return(reply_error(4));

			value = get_word(&phy_receive_buffer[1]);
			eeprom_write_uint16(&eeprom->temp_cal[input_io].multiplier, value);

			value = get_word(&phy_receive_buffer[3]);
			eeprom_write_uint16(&eeprom->temp_cal[input_io].offset, value);

			return(reply_ok());
		}

		case(0xf0):	// read temperature sensor calibration values
		{
			int32_t value;
			uint8_t replystring[4];

			if(input_io >= TEMP_PORTS)
				return(reply_error(3));

			value = eeprom_read_uint16(&eeprom->temp_cal[input_io].multiplier);
			put_word(value, &replystring[0]);

			value = eeprom_read_uint16(&eeprom->temp_cal[input_io].offset);
			put_word(value, &replystring[2]);

			return(reply(0, sizeof(replystring), replystring));
		}

		default:
		{
			return(reply_error(2));
		}
	}

	return(reply_error(2));
}

void if_idle(void) // gets called ~500 Hz
{
	static uint16_t led_divisor = 0;

	if(led_divisor > 1024) // 500 Hz / 1024 = 0.5 Hz = 2 sec
	{
		led_divisor = 0;

		*internal_output_ports[1].port |= _BV(internal_output_ports[1].bit);
		if_sense_led  = if_active_led_timeout;
	}

	led_divisor++;

	if(adc_warmup > 0)
	{
		adc_warmup--;
		adc_start();
	}
	else
	{
		if(adc_ready() && (adc_samples < 1024))
		{
			adc_samples++;
			adc_value += adc_read();
			adc_start();
		}
	}

	watchdog_reset();
}

int main(void)
{
	uint8_t slot;

	watchdog_stop();

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
		*input_ports[slot].port			&= ~_BV(input_ports[slot].bit); // low
		*input_ports[slot].ddr			&= ~_BV(input_ports[slot].bit);	// input
		*input_ports[slot].port			&= ~_BV(input_ports[slot].bit); // disable pullup
		*input_ports[slot].pcmskreg		|=  _BV(input_ports[slot].pcmskbit);
		input_ports[slot].state			=	*input_ports[slot].pin & _BV(input_ports[slot].bit);
		input_ports[slot].counter		=	0;
		GIMSK							|=  _BV(input_ports[slot].gimskbit);
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		*output_ports[slot].ddr		|=  _BV(output_ports[slot].bit);
		*output_ports[slot].port	&= ~_BV(output_ports[slot].bit);
	}

	for(slot = 0; slot < INTERNAL_OUTPUT_PORTS; slot++)
	{
		*internal_output_ports[slot].ddr	|= _BV(internal_output_ports[slot].bit);
		*internal_output_ports[slot].port	&= ~_BV(internal_output_ports[slot].bit);
	}

	adc_init();

	// 16 mhz / 256 / 256 = 244 Hz
	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_compa(0x00);
	timer0_set_compb(0x00);
	timer0_start();

	// 16 mhz / 64 / 1024 = 244 Hz
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

	adc_warmup	= adc_warmup_init;
	adc_samples	= 0;
	adc_value	= 0;

	watchdog_setup(WATCHDOG_PRESCALER_1024K);	//	8.0 seconds timeout
	watchdog_start();

	sei();

	usi_twi_slave(0x02, 1, process_input, if_idle);
}
