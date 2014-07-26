/**																		  ___
	@File	: Thermalmanagement.c				(PCINT5/RST/ADC0/dW) PB5-|*	 |-VCC	[2:5] V.							Spec
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
  @Original 14/06/2014
  @Update	25/07/2014
*/

 /*************************\
 | Compilation time option |
 \*************************/

#define PWM_VAL_MIN		0x19	//Minimum PWM value	[0:FF]
#define PWM_VAL_MAX		0xFF	//Maximum PWM value	[0:FF]
#define TEMP_VAL_START	25		//Temperature to start Fan power[0:100] °C
#define TEMP_VAL_MAX	100		//Maximum temperature the user can set the Max_temp

//Uncomment if applicable
#define PWM_ACTIVE_LOW		
//#define FAN_POWER_ACTIVE_LOW
//#define ALERT_ACTIVE_LOW


 /**********\
 | Includes |
 \**********/

#define F_CPU 1000000UL  // 1 MHz => usefull for delay
#include <stdint.h>
#include <stdlib.h>

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>


 /********************\
 | MACRO and shortcut |
 \********************/

#define MUX_POT_CHAN	0x23	//(ADMUX = (1<< ADLAR) | 0x02)//ADC3	PB3
#define MUX_TEMP_CHAN	0x22	//(ADMUX = (1<< ADLAR) | 0x03)//ADC2	PB4

//ADC Conversion Done
#define AdcConvDone		((ADCSRA & (1 << ADSC)) == 0)

//fan power control [on/ off]
#ifdef FAN_POWER_ACTIVE_LOW
	#define fan_power_on() 		(PORTB	&= ~(1<<PORTB1))
	#define fan_power_off() 	(PORTB	|=  (1<<PORTB1))
#else
	#define fan_power_off() 	(PORTB	&= ~(1<<PORTB1))
	#define fan_power_on() 		(PORTB	|=  (1<<PORTB1))
#endif

//Alert Led macro
#ifdef ALERT_ACTIVE_LOW  
	#define led_on() 		(PORTB	&= ~(1<<PORTB2))
	#define led_off() 		(PORTB	|=  (1<<PORTB2))
#else
	#define led_off() 		(PORTB	&= ~(1<<PORTB2))
	#define led_on() 		(PORTB	|=  (1<<PORTB2))
#endif

 /************\
 | Prototypes |
 \************/

//Blink the led to tell user max temperature allowed (step of 10°C)
void blink(unsigned int numbers);

//Initialize register required for the application
void ioinit (void);

//Read temperature ADC and return value in °C
void ReadTemperature(void);

//Read pot ADC and return value between [0:100]
void ReadPot(void);

//return the PWM value for actual temperature
int LinearPwmCalculate(void);

//Global Variable
unsigned int temperature		=0;		// Actual temperature in °C
unsigned int temperature_max	=0;		// user selected max allowed temperature

int main(void)
{
	int temperature_max_mem	=0;		// last temperature_max_mem
	int switch_adc			=0;		// ADC channel change flag

	_delay_ms(1000);
	
	ioinit();	//initialize all IO
  	
	while(1)
	{
		  /////////////////
		 // ADC Reading //
		/////////////////

		//If last ADC conversion is done			
		if(AdcConvDone)
		{	
			
			//select next adc reading channel (read 3x temp for each pot read
			switch_adc++;
			switch_adc &= 0x03;

			switch(switch_adc)
			{
				case 0:		ReadTemperature();	break;
				case 1:		ReadTemperature();	break;
				case 2:		ReadTemperature();	ADMUX = MUX_POT_CHAN;	break;
				case 3:		ReadPot();			ADMUX = MUX_TEMP_CHAN;	break;	
				default:	switch_adc=0;		ADMUX = MUX_TEMP_CHAN;	break; 
			}
			//start new conversion
			ADCSRA |= (1<< ADSC);			
		}
	   

		  ////////////////////////////////////////
		 // Set the maximum allowed temperature//
		////////////////////////////////////////

		//if user selected a new value
		//The led turn off for 1 sec and
		//then blink for each 10°C increment
		//ex: 5 blink = 50°C
		//to keep if from continuously changing,
		//the new value must be 8°C more than 
		//the old value to change the value
		if((temperature_max>>1) != temperature_max_mem)
		{
			temperature_max_mem = temperature_max>>1;
			led_off();
			_delay_ms(1000);
			blink(temperature_max/10);
			_delay_ms(1000);
		}
		
  
		  ///////////////////
		 // PWM adjusting //
		/////////////////// 

		if(temperature >= temperature_max)
		{
			fan_power_on();
			led_on();

			#ifdef PWM_ACTIVE_LOW
				OCR0A =  0xFF-PWM_VAL_MAX;
			#else
				OCR0A =  PWM_VAL_MAX;
			#endif	
		}
		else
		{ 
			led_off();
			if (temperature<=TEMP_VAL_START)
			{
				fan_power_off();
				
				#ifdef PWM_ACTIVE_LOW
					OCR0A = 0xFF;
				#else
					OCR0A = 0x00;
				#endif
				
			} 
			else
			{	
				fan_power_on();
				
				#ifdef PWM_ACTIVE_LOW
					OCR0A = 0xFF-LinearPwmCalculate();
				#else
					OCR0A = LinearPwmCalculate();	
				#endif			
			}
		}				
	}
}


//Blink the led to tell user max temperature allowed (step of 10°C)
void blink(unsigned int numbers){
	while(numbers--){
		led_on();
		_delay_ms(200);
		led_off();
		_delay_ms(200);
	}
}

//Initialize register required for the application
void ioinit (void){

	////////////////////////////////////////////////////////////////////////////
	//General I/O
	DIDR0 	=(1<<AIN0D)	|(1<<AIN1D)	|(1<<ADC1D)	|(1<<ADC3D)	|(1<<ADC2D)	|(1<<ADC0D);
	DDRB 	=(1<<DDB0)	|(1<<DDB1)	|(1<<DDB2);

	////////////////////////////////////////////////////////////////////////////
	//ADC
	ADCSRA	=  (1<<ADPS1)	|(1<<ADPS0)	|(1<<ADEN);
	ADMUX	= MUX_POT_CHAN;

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

void ReadTemperature(void)
{
	temperature= (ADCH-25)<<1;
}

//return pot value [0:100]
void ReadPot(void)
{
	unsigned int temp;
	temp= ADCH/2;

	if(temp>TEMP_VAL_MAX)
		temperature_max= TEMP_VAL_MAX;
	else
		temperature_max= temp;
}


//The pwm linearisation formulae is
//pwm = (temperature-temp_start) * (PWM_MAX-PWM_Min) / (temperature_max-temp_start) + PWM_Min
int LinearPwmCalculate(void)
{
	double temp_math;
	
//	temp_math  = ( temperature - TEMP_VAL_START);
//	temp_math *= ( PWM_VAL_MAX - PWM_VAL_MIN);
//	temp_math /= ( temperature_max - TEMP_VAL_START);
//	temp_math += ( PWM_VAL_MIN);

	temp_math  = ( temperature - TEMP_VAL_START)*( PWM_VAL_MAX - PWM_VAL_MIN) /  ( temperature_max - TEMP_VAL_START)+ ( PWM_VAL_MIN);


	//Should never be greater than 0xFF from my calculation
	if(temp_math>= 0xFF)
		return 0xFF;

	return ((unsigned int) temp_math);
}
