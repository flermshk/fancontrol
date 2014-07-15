/**																		  ___
	@File	: main.c							(PCINT5/RST/ADC0/dW) PB5-|*	 |-VCC	[2:5] V.							Spec
	@project: Smart fan controller				(PCINT3/CLKI/ADC3)	 PB3-|	 |-PB2	(SCK/ADC1/T0/PCINT2)				1KB Flash
	@Brief	: Programmable fan controller		(PCINT4/ADC2)		 PB4-|	 |-PB1	(MISO/AIN1/OC0B/INT0/PCINT1)		64B SRAM
	@Hardware ATtiny13A												 GND-|___|-PB0	(MOSI/AIN0/OC0A/PCINT0)				64B EEPROM
							  
	Port   Tris		- ALT	- Role
	- - - - - - - - - - - - - - 
	PB0	 - Output	- OC0A	- PWM (fan control) [@25KHz]
	PB1	 - Output	-		- Reg_control (on/off)
	PB2	 - Output	-		- LED (alarm) 
	PB3	 -			- ADC3	- potentiometer
	PB4	 -			- ADC2	- temp sensor
	PB5	 - 			- dont	- touch
	
  @Author: Jonathan L'Espérance
  @Date 14/06/14
*/
///////////////////////////////////////////////
//Temperature response section
const char LUT[2][21]= 
{	
	{20,	25,		30,		35,		40,		45,		50,		55,		60,		65,		70,		75,		80,		85,		90,		95,		100,	105,	110,	115,	120 },	/*Temperatures*/
	{0x00,	0x19,	0x33,	0x4C,	0x66,	0x72,	0x7F,	0x99,	0xB2,	0xCC,	0xE5,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF,	0xFF}	/*PWM Value*/
};


#define sensors //


#define F_CPU 1000000UL  // 1 MHz
#include <stdint.h>
#include <stdlib.h>

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

//Personal macro
#define adc_channel_pot		0x23	//(ADMUX = (1<< ADLAR) | 0x02)//ADC3  PB3
#define adc_channel_temp	0x22	//(ADMUX = (1<< ADLAR) | 0x03)//ADC2	PB4

#define reg_off() 		(PORTB	&= ~(1<<PORTB0))
#define reg_on() 		(PORTB	|=  (1<<PORTB0))

#define pwr_off() 		(PORTB	&= ~(1<<PORTB1))
#define pwr_on() 		(PORTB	|=  (1<<PORTB1))

#define led_off() 		(PORTB	&= ~(1<<PORTB2))
#define led_on() 		(PORTB	|=  (1<<PORTB2))

void blink();

void ioinit (void){
	////////////////////////////////////////////////////////////////////////////
	//General I/O
	DIDR0 	=(1<<AIN0D)	|(1<<AIN1D)	|(1<<ADC1D)	|(1<<ADC3D)	|(1<<ADC2D)	|(1<<ADC0D);
	DDRB 	=(1<<DDB0)	|(1<<DDB1)	|(1<<DDB2);

	////////////////////////////////////////////////////////////////////////////
	//ADC
	ADCSRA	=  (1<<ADPS1)	|(1<<ADPS0)	|(1<<ADEN);
	ADMUX	= adc_channel_pot;

	////////////////////////////////////////////////////////////////////////////
	//EEPROM
	//EEPROM Control Register
	//EECR 	=(0<<EERE)	|(0<<EEWE)	|(0<<EEMWE)	|(0<<EERIE)	|(0<<EEPM0)	|(0<<EEPM1);
	//EEPROM Data Register
	//EEDR 	=0;
	//EEPROM Address Register
	//EEARL	= 0;

	////////////////////////////////////////////////////////////////////////////
	//Timer / PWM
	TCCR0A 	=(1<<WGM01)|(1<<WGM00)|(1<<COM0A1)|(1<<COM0A0);		
	OCR0A 	= 0;	
	TCCR0B 	=(0<<CS02)|(0<<CS01)|(1<<CS00);//|   (1<<WGM02);		//gm02 toggle pwm
}

unsigned int readadc(int channel);

  int main(void){
  	
	int temperature=0, alertlvl=0, oldalertlvl=0, switch_adc=0;
  	int i=0;
  	 _delay_ms(1000);
	ioinit();	//initialize all IO
  	while(1){
		////////////////////////////////////////////////////////////
		//ADC reading process
		if((ADCSRA & (1 << ADSC)) == 0){	//si conversion terminé
		
			switch_adc = ~switch_adc;	//read other channel
			if(switch_adc & 0x01){
				temperature = (ADCH-25)<<1; //temp ~ 2*(adc-25)  <-- verified, 
				ADMUX	= adc_channel_pot;
			}else{
				alertlvl = ADCH/20;
				ADMUX	= adc_channel_temp;
			}
			ADCSRA |= (1<< ADSC);			//Start conversion
	}
		   
		   ////////////////////////////////////////////////////////////
		  //Setup du alert level
			if(alertlvl != oldalertlvl){
				oldalertlvl = alertlvl;
				led_off();
				_delay_ms(1000);
				blink(alertlvl);
				_delay_ms(1000);
			}
		  
		   ////////////////////////////////////////////////////////////
		   //PWM ajusting
		   for(i=0 ; temperature>LUT[0][i] ; i++);
	   
	   		OCR0A = ~LUT[1][i];

			////////////////////////////////////////////////////////////
			//Alert monitoring
			if(alertlvl<(temperature/10))
				led_on();
			else
				led_off();
	}
}


void blink(unsigned int numbers){
	while(numbers--){
		led_on();
		_delay_ms(200);
		led_off();
		_delay_ms(200);
	}
}


//	void EEPROM_write(unsigned char ucAddress, unsigned char ucData)
//	{
//		/* Wait for completion of previous write */
//		while(EECR & (1<<EEWE));
//		
//		/* Set Programming mode */
//		EECR = (0<<EEPM1)|(0<<EEPM0)
//		
//		/* Set up address and data registers */
//		EEARL = ucAddress;
//		EEDR = ucData;
//		
//		/* Write logical one to EEMWE */
//		EECR |= (1<<EEMWE);
//		
//		/* Start eeprom write by setting EEWE */
//		EECR |= (1<<EEWE);
//	}
//	
//	unsigned char EEPROM_read(unsigned char ucAddress)
//	{
//		/* Wait for completion of previous write */
//		while(EECR & (1<<EEWE))
//		;
//		/* Set up address register */
//		EEARL = ucAddress;
//		/* Start eeprom read by writing EERE */
//		EECR |= (1<<EERE);
//		/* Return data from data register */
//		return EEDR;
//	}