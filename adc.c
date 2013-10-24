#include <avr/io.h>

#include "ioports.h"
#include "adc.h"

void adc_init(void)
{
	ADCSRA	&=	~_BV(ADEN);			//	disable ADC

	ACSRA =		(1 << ACD)		|	// disable comparator
				(0 << ACBG)		|	// !bandgap select (n/a)
				(0 << ACO)		|	// !enable analog comparator output
				(1 << ACI)		|	// clear comparator interrupt flag
				(0 << ACIE)		|	// !enable analog comparator interrupt
				(0 << ACME)		|	// !use adc multiplexer
				(0 << ACIS1)	|	// !interrupt mode select (n/a)
				(0 << ACIS0);

	ACSRB =		(0 << HSEL)		|	// !hysteresis select (n/a)
				(0 << HLEV)		|	// !hysteresis level (n/a)
				(0 << 5)		|
				(0 << 4)		|
				(0 << 3)		|
				(0 << ACM0)		|	// !analog comparator multiplexer (n/a)
				(0 << ACM1)		|	// !analog comparator multiplexer (n/a)
				(0 << ACM2);		// !analog comparator multiplexer (n/a)

	ADCSRA =	(0	<< ADEN)	|	// !enable ADC
				(0	<< ADSC)	|	// !start conversion
				(0	<< ADATE)	|	// !auto trigger enable
				(1	<< ADIF)	|	// clear interrupt flag
				(0	<< ADIE)	|	// !enable interrupt
				(1	<< ADPS2)	|
				(1	<< ADPS1)	|
				(0	<< ADPS0);		// select clock scaler 110 = 64 = ADC runs on 122 kHz.

	ADCSRB	=	(0	<< BIN)		|	// !bipolair input
				(0	<< GSEL)	|	// gain select (n/a)
				(0	<< 5)		|	// reserved
				(0	<< REFS2)	|	// 1.1V internal ref
				(0	<< MUX5)	|	// input 1.1V
				(0	<< ADTS2)	|
				(0	<< ADTS1)	|	// auto trigger source (n/a)
				(0	<< ADTS0);

	ADMUX	=	(1	<< REFS1)	|	// 1.1V internal ref
				(0	<< REFS0)	|	// 1.1V internal ref
				(0	<< ADLAR)	|	// right adjust result
				(1	<< MUX4)	|	// input 1.1V
				(1	<< MUX3)	|	// input 1.1V
				(1	<< MUX2)	|	// input 1.1V
				(1	<< MUX1)	|	// input 1.1V
				(0	<< MUX0);		// input 1.1V

	ADCSRA	|=	_BV(ADEN);			//	enable ADC
}

void adc_select(const adcport_t *p)
{
	ADMUX	=	(1			<< REFS1)	|	// 1.1V internal ref
				(0			<< REFS0)	|	// 1.1V internal ref
				(0			<< ADLAR)	|	// right adjust result
				(p->mux[4]	<< MUX4)	|
				(p->mux[3]	<< MUX3)	|
				(p->mux[2]	<< MUX2)	|	// input
				(p->mux[1]	<< MUX1)	|
				(p->mux[0]	<< MUX0);

	ADCSRB	=	(0			<< BIN)		|	// unipolair input
				(0			<< GSEL)	|	// gain select (n/a)
				(0			<< 5)		|	// reserved
				(0			<< REFS2)	|	// 1.1V internal ref
				(p->mux[5]	<< MUX5)	|
				(0			<< ADTS2)	|
				(0			<< ADTS1)	|	// auto trigger source (n/a)
				(0			<< ADTS0);

	ADCW = 0;
}
