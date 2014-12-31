//==============================================================================================================================
// U S B   R E F L O W   O V E N   C O N T R O L L E R
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Reflow.c"
// Title 			: USB Reflow Oven Controller
// Date 			: 18 Jul 2011
// Version 		: 1.00
// Target MCU : ATMEGA32U2
// Author			: Simon Ratcliffe


//==============================================================================================================================
// Description of port usage
//
// Port/Bit	Direction	Usage
// --------	---------	----------------------------------------------------------------------------------------------------------
// B.0			Output		SPI SS for MAX6675
// B.1			Output		SPI Clock to MAX6675				(SCK)
// B.2			Output																(MOSI)
// B.3			Input			SPI Input from MAX6675			(MISO)
// B.4			Output		Dynamic Element Control (SSR)
// B.5			Output		Oven Power Cuttoff (EMR)
// B.6			Output		Spare output
// B.7			Output		Spare output
//
// C.4			Input			Switch 1
// C.5			Input			Switch 2
// C.6			Input			Switch 3
// C.7			Input			Switch 0
//
// D.0			Output		LCD DB4
// D.1			Output		LCD DB5
// D.2			Output		LCD DB6
// D.3			Output		LCD DB7
// D.4			Output		LCD RS
// D.5			Output		LCD R/W
// D.6			Output		LCD E
// D.7			Output		Buzzer

//==============================================================================================================================
// Includes

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

#include "Descriptors.h"
#include "../LUFA/LUFA/Version.h"
#include "../LUFA/LUFA/Drivers/USB/USB.h"

#include "ReflowOven.h"
#include "lcd.h"
#include "spi.h"
#include "menu.h"
#include "version.h"

//==============================================================================================================================
// Protect against initial watchdog timeouts

void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));

void get_mcusr(void)
{
	MCUSR = 0;
	wdt_disable();
}

//==============================================================================================================================
// External variables

extern uint8_t CurrentMenuItemIdx;

//==============================================================================================================================
// Defines

#define OVEN_RELAY_SSR		PB4
#define OVEN_RELAY_EMR		PB5

#define EMR_ON						PORTB &= ~_BV(OVEN_RELAY_EMR)
#define EMR_OFF						PORTB |= _BV(OVEN_RELAY_EMR)

#define SSR_ON						PORTB |= _BV(OVEN_RELAY_SSR)
#define SSR_OFF						PORTB &= ~_BV(OVEN_RELAY_SSR)

//==============================================================================================================================
// Typedefs

typedef struct
{
	char name[PROFILE_NAME_LEN];
	unsigned char calibrated;
	unsigned char preheat_temp;
	unsigned char soak_dutycycle;
	unsigned char soak_temp;
	unsigned char reflow_time;
	unsigned char reflow_temp;
	unsigned char preheat_cutoff;
	unsigned char reflow_cutoff;
} __profile;

//==============================================================================================================================
// EEPROM Variables and Data

uint8_t __attribute__((section(".validapp"))) ValidApp = 0xBB;

uint8_t EEMEM OvenCalibrated = 1;
uint8_t EEMEM ProfileCount = 2;

__profile EEMEM Profiles[MAX_PROFILES] = 
{
	{"Default         ",1,150,8,180,90,215,131,206},			//perfect for leaded solder
	{"Bigger Board    ",1,150,12,180,120,220,138,212},
	{"Leadfree        ",1,150,12,200,120,255,138,248} 
};

//__profile EEMEM Profiles[MAX_PROFILES] = {{"Leadfree",1,150,3,2,200,120,255,138,248}}; //perfect for leaded solder
// original production profile that was used in the first 2 years __profile EEMEM Profiles[MAX_PROFILES] = {{"Default",1,150,2,4,180,90,215,124,204}};
// setup __profile EEMEM Profiles[MAX_PROFILES] = {{"Default",0,150,2,4,180,90,220,0,0}};
// 210 reflow __profile EEMEM Profiles[MAX_PROFILES] = {{"Default",1,150,2,4,180,90,210,123,199}};

uint8_t EEMEM TempCounts[20] = {18,14,14,15,11,10,11,11,10,12,11,12,12,11,12,13,18,15,16,16};

//==============================================================================================================================
// PROGMEM Menu definition

const MENU_ITEM ShowReflowMenu[];
const MENU_ITEM SettingsMenu[];
const MENU_ITEM SelectProfileMenu[];

const char Str00[] PROGMEM = "Main menu       ";
const char Str01[] PROGMEM = "Reflow          ";
const char Str02[] PROGMEM = "Settings        ";

const MENU_ITEM MainMenu[] PROGMEM = 
{
	{MENU_ITEM_TYPE_MAIN_MENU_HEADER, Str00, (PGM_P)SetIdleMode},
	{MENU_ITEM_TYPE_SUB_MENU, Str01, (PGM_P)ShowReflowMenu},
	{MENU_ITEM_TYPE_SUB_MENU, Str02, (PGM_P)SettingsMenu},
	{MENU_ITEM_TYPE_END_OF_MENU, NULL, 0}
};

const char Str10[] PROGMEM = "Reflow          ";
const char Str11[] PROGMEM = "Run             ";
const char Str12[] PROGMEM = "Select Profile  ";
const char Str13[] PROGMEM = "Edit Profile    ";

const MENU_ITEM ShowReflowMenu[] PROGMEM =
{	
	{MENU_ITEM_TYPE_SUB_MENU_HEADER, Str10, (PGM_P)MainMenu},
	{MENU_ITEM_TYPE_COMMAND, Str11, (PGM_P)RunProfileCommand},
	{MENU_ITEM_TYPE_SUB_MENU, Str12, (PGM_P)SelectProfileMenu},
	{MENU_ITEM_TYPE_COMMAND, Str13, (PGM_P)EditProfileCommand},
	{MENU_ITEM_TYPE_END_OF_MENU, NULL, 0}
};

const char Str200[] PROGMEM = "Select Profile  ";

const MENU_ITEM SelectProfileMenu[] PROGMEM =
{
	{MENU_ITEM_TYPE_SUB_MENU_HEADER, Str200, (PGM_P)ShowReflowMenu},
	{MENU_ITEM_TYPE_EEMEM_COMMAND, (PGM_P)MenuGetEEMEMItem, (PGM_P)SelectProfileCommand}
};	

const char Str20[] PROGMEM = "Settings        ";
const char Str21[] PROGMEM = "Calib. Oven     ";
const char Str22[] PROGMEM = "Calib. Profile  ";

const MENU_ITEM SettingsMenu[] PROGMEM =
{
	{MENU_ITEM_TYPE_SUB_MENU_HEADER, Str20, (PGM_P)MainMenu},
	{MENU_ITEM_TYPE_COMMAND, Str21, (PGM_P)CalibrateOvenCommand},
	{MENU_ITEM_TYPE_COMMAND, Str22, (PGM_P)CalibrateProfileCommand},
	{MENU_ITEM_TYPE_END_OF_MENU, NULL, 0}
};

// Oven calibration stages

// Profile calibration stages

// Reflow stages

//==============================================================================================================================
// Global Variables

USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
	.Config =
	{
		.ControlInterfaceNumber   = 0,
		.DataINEndpoint           =
		{
			.Address          = CDC_TX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.DataOUTEndpoint =
		{
			.Address          = CDC_RX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.NotificationEndpoint =
		{
			.Address          = CDC_NOTIFICATION_EPADDR,
			.Size             = CDC_NOTIFICATION_EPSIZE,
			.Banks            = 1,
		},
	},
};

static FILE USBSerialStream;
char inBuf[80];
char tmpStr[17];
char profileName[PROFILE_NAME_LEN];
uint8_t currentProfile = 1;
bool usbConnected = false;
bool isRunning = false;
bool showTemp = true;
uint16_t ovenTemp;
uint16_t ovenTempArray[68];
int16_t ovenDelta4;
int16_t ovenDelta16;
int16_t ovenDelta64;
uint8_t deltaCount = 0;
uint8_t ovenStage; // Used to store where a task is up to
uint16_t ovenCounter;
void(*ProcessHandler)();
uint16_t count = 0;
uint16_t endCount = 3600;
uint8_t endSet = 0;
__profile profile;
volatile uint8_t tick = 0;
volatile uint8_t subtickCounter = 0;
volatile uint8_t duty_cycle = 0;
bool lcdPresent = true;
uint8_t buttons = 0;
uint8_t newButton;

//==============================================================================================================================
// Interrupt routines

ISR (TIMER1_COMPA_vect)
{
	subtickCounter++;
	if (subtickCounter == 10)
	{
		tick++;
	}	
	if (subtickCounter == 20)
	{
		subtickCounter = 0;
		tick++;
	}	
	
	if ((duty_cycle > 0) && (duty_cycle < 20))
	{
		if (subtickCounter == duty_cycle)
		{
			SSR_OFF;
		}
		else if (subtickCounter == 0)
		{
			SSR_ON;
		}
	}
}

//==============================================================================================================================
// Get the menu item at a given index from EEMEM

void MenuGetEEMEMItem(uint8_t idx)
{
	eeprom_read_block ((void*)&profileName, (const void*)&Profiles[idx-1], PROFILE_NAME_LEN);
}

//==============================================================================================================================
// Event handler for the library USB Connection event

void EVENT_USB_Device_Connect(void)
{
	usbConnected = true;
}

//==============================================================================================================================
// Event handler for the library USB Disconnection event

void EVENT_USB_Device_Disconnect(void)
{
	usbConnected = false;
}

//==============================================================================================================================
// Event handler for the library USB Configuration Changed event

void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

//==============================================================================================================================
// Event handler for the library USB Control Request reception event

void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

//==============================================================================================================================
// Functions

uint8_t GetPacket(char* buf, uint8_t size)
{
	if (CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface))
	{
		fgets(buf, size, &USBSerialStream);
//		printf("$$%s$$\n", buf);
		while ((strlen(buf) > 0) && ((buf[strlen(buf)-1] == '\r') || (buf[strlen(buf)-1] == '\n')))
		{
			buf[strlen(buf)-1] = 0;
		}
//		printf("##%s##\n", inBuf);
		return true;
	}
	
	return false;
}

//==============================================================================================================================
// Configure the various bits of hardware

void SetupHardware(void)
{
	// Configure IO Ports
	PORTB = _BV(OVEN_RELAY_EMR); // Turn off the EMR relay
	DDRB |= _BV(OVEN_RELAY_SSR) | _BV(OVEN_RELAY_EMR); // Relay outputs
	DDRD |= _BV(PD7); // Buzzer as output
	PORTC = 0xF0; // Enable pullups on for switches

	// Initialise SPI
  spi_init ();

	//Initialise LCD
  if (!lcd_init (LCD_DISP_ON)) // Initialise the LCD
  {
	  lcdPresent = false;
  }

	// USB Hardware Initialization
	USB_Init();
	
	// Setup Timer/Counter1
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 6250; // = 0.05 seconds
//	OCR1A = 12500; // = 0.1 seconds
//	OCR1A = 62500; // = 0.5 seconds
	TIMSK1 = (1 << OCIE1A);
}

//==============================================================================================================================
// Send a temperature packet

void UpdateTemp (void)
{
	char str[20];

	ovenTemp = spi_read ();
	if (ovenTempArray[0] == 0)
	{
		for (uint8_t i = 0; i < 68; i++)
		{
			ovenTempArray[i] = ovenTemp;
		}
	}
	else
	{
		memmove(&ovenTempArray[1], &ovenTempArray[0], sizeof(uint16_t)*67);
		ovenTempArray[0] = ovenTemp;
	}
	
	ovenDelta4 = (int16_t)(ovenTempArray[0]+ovenTempArray[1]+ovenTempArray[2]+ovenTempArray[3]-ovenTempArray[8]-ovenTempArray[9]-ovenTempArray[10]-ovenTempArray[11]+4)>>3;
	ovenDelta16 = (int16_t)(ovenTempArray[0]+ovenTempArray[1]+ovenTempArray[2]+ovenTempArray[3]-ovenTempArray[32]-ovenTempArray[33]-ovenTempArray[34]-ovenTempArray[35]+4)>>3;
	ovenDelta64 = (int16_t)(ovenTempArray[0]+ovenTempArray[1]+ovenTempArray[2]+ovenTempArray[3]-ovenTempArray[64]-ovenTempArray[65]-ovenTempArray[66]-ovenTempArray[67]+4)>>3;

	if (lcdPresent)
	{
		lcd_gotoxy(10, 0);
	}

	if (ovenTemp >= 65533)
	{
	  set_duty_cycle(0); //Turn off SSR
	  _delay_ms(25); //Wait for SSR to power down
	  EMR_OFF; // Turn off EMR
		if (ovenTemp == 65535)
		{
			sprintf_P (str, PSTR("No TC "));
		}
		else if (ovenTemp == 65534)			
		{
			sprintf_P (str, PSTR("SG Err"));
		}
		else
		{
			sprintf_P (str, PSTR("SV Err"));
		}
		if ((lcdPresent) && (showTemp))
		{
			lcd_puts (str);
		}		
		fprintf (&USBSerialStream, "%u,%u,%s\n", ovenStage, count, str);
	}
	else
	{
		sprintf_P (str, PSTR("%3u.%02u"), ovenTemp>>2, (ovenTemp & 0x03)*25);
		if ((lcdPresent) && (showTemp))
		{
			lcd_puts (str);
		}
		fprintf_P (&USBSerialStream, PSTR("%u,%u,%s,%u,%d,%d,%d\n"), ovenStage, count, str, duty_cycle, ovenDelta4, ovenDelta16, ovenDelta64);
	}

	count++;
}

//==============================================================================================================================
// Send the oven settings

void SendOvenSettings(void)
{
	char ReportString[80];
	
	sprintf_P(ReportString, PSTR("=OGET,%u,%u,%u"), eeprom_read_byte(&OvenCalibrated), MAX_PROFILES, eeprom_read_byte(&ProfileCount));
	for (uint8_t i = 0; i < 20; i++)
	{
		sprintf_P(tmpStr, PSTR(",%u"), eeprom_read_byte(&TempCounts[i]));
		strcat(ReportString, tmpStr);
	}
	
	strcat(ReportString,"\n");
	
	// Write the string to the virtual COM port via the created character stream
	fputs(ReportString, &USBSerialStream);
}

//==============================================================================================================================
// Cleanup and jump into the bootloader

void Bootloader(void)
{
	lcd_clrscr();
	lcd_puts_P("Bootloader");
//	wdt_disable();
	TIMSK1 = 0; // Disable TIMER1
	USB_Detach();
	_delay_ms(1000);
	eeprom_write_byte(&ValidApp, 0xFF);
	while (!eeprom_is_ready());
	wdt_enable(WDTO_30MS);
	for (;;);
}

//==============================================================================================================================
//

void set_duty_cycle (uint8_t ratio)
{
	duty_cycle = ratio;
	if (duty_cycle == 0)
	{
		SSR_OFF;
	}
	if (duty_cycle == 20)
	{
		SSR_ON;
	}
}

//==============================================================================================================================
//

void printProfile (void)
{
	fprintf (&USBSerialStream, "=RUN,%s,%u,%u,%u,%u,%u,%u,%u,%u\n",
		profile.name, profile.preheat_temp, profile.soak_dutycycle*5, profile.soak_temp, profile.reflow_time, profile.reflow_temp, profile.calibrated,
		profile.preheat_cutoff, profile.reflow_cutoff);
}

//==============================================================================================================================
//

void RunProfileCommand()
{
	lcd_gotoxy(0, 1);
	lcd_puts_P("                ");
	showTemp = true;
	ovenStage = 0;
	count = 0;
	ProcessHandler = RunProfileHandler;
	isRunning = true;
};

//==============================================================================================================================
//

void RunProfileHandler()
{
	switch (ovenStage)
	{
		case 0: // close door & start message
			lcd_gotoxy(0, 1);
			lcd_puts_P("Close door     ");
			ovenStage++;
			break;

		case 1: // wait for button press
			if ((newButton) && (buttons == EVENT_ENTER_BUTTON_PUSHED))
			{
				ovenStage++;
			}
			break;

		case 2: // Preheat
			printProfile(); // Send the profile we are about to run to the serial port
			lcd_gotoxy(0, 1);
			lcd_puts_P("Preheat        ");
			count = 0;
			EMR_ON; //Turn on the EMR
			_delay_ms(25);
			set_duty_cycle(20); //Turn on the SSR
			ovenStage++;
			break;

		case 3: // Reached preheat cutoff
			if ((ovenTemp >> 2) >= profile.preheat_cutoff)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("Preheat cutoff   ");
				set_duty_cycle(0); //Turn off the SSR
				ovenCounter = 3 * eeprom_read_byte(&TempCounts[9]);
				ovenStage++;
			}
			break;

		case 4: // Wait for overshoot timeout
			if (tick)
			{
				ovenCounter--;
			}				

			if (!ovenCounter)
			{
				char str[17];
				set_duty_cycle(profile.soak_dutycycle);
				sprintf_P(str, PSTR("Duty cycle %3u%%"), profile.soak_dutycycle*5);
				lcd_gotoxy(0, 1);
				lcd_puts(str);
				ovenStage++;
			}
			break;

		case 5:
			if ((ovenTemp >> 2) >= profile.soak_temp)
			{
				set_duty_cycle(20);
				lcd_gotoxy(0, 1);
				lcd_puts_P("Reflow         ");
				ovenCounter = count;
				ovenStage++;
			}
			else
			{
				if ((ovenTemp >> 2) <= profile.preheat_temp)
				{
					ovenCounter = count;
				}
			}
			break;

		case 6: // Reached reflow cutoff
			if ((ovenTemp >> 2) >= profile.reflow_cutoff)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("Reflow cutoff ");
				set_duty_cycle(0);
				ovenStage++;
			}
			break;

		case 7:
			if ((ovenTemp >> 2) <= profile.reflow_cutoff)
			{
				set_duty_cycle(4); // just a touch more heat
			}
			else
			{
				set_duty_cycle(3); // maintain the temp
			}
			if ((count-ovenCounter) >= (profile.reflow_time << 1))
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("Open door    ");
				set_duty_cycle(0); // turn the heat off
				_delay_ms(25);
				EMR_OFF;
				ovenStage++;
				ovenCounter = 0;
			}
			break;

		case 8:
		  if (tick)
		  {
				if (ovenCounter < 20)
				{
					if (ovenCounter % 2)
					{
						PORTD &= ~_BV(7); // Buzzer off
					}
					else
					{
						PORTD |= _BV(7); // Buzzer on
					}
					ovenCounter++;
				}
				else
				{
					ovenStage++;
				}
		  }				
			break;

		case 9:
			if ((ovenTemp >> 2) < 100)
			{
				fprintf(&USBSerialStream, "=END\n");
				isRunning = false;
				ovenStage = 0;
				SetIdleMode();
			}
			break;
	}
};

//==============================================================================================================================
//

void SelectProfileCommand()
{
	currentProfile = CurrentMenuItemIdx;
	eeprom_read_block ((void*)&profile, (const void*)&Profiles[currentProfile-1], sizeof(__profile));
};

//==============================================================================================================================
//

void EditProfileCommand()
{
};

//==============================================================================================================================
//
	
void CalibrateOvenCommand()
{
	lcd_gotoxy(0, 1);
	lcd_puts_P("                ");
	showTemp = true;
	ovenStage = 0;
	count = 0;
	ProcessHandler = CalibrateOvenHandler;
	isRunning = true;
};

//==============================================================================================================================
//
/*
void CalibrateOvenHandler()
{
	switch (ovenStage)
	{
		case 0: // close door & start message
			lcd_gotoxy(0, 1);
			lcd_puts_P("Close door     ");
			ovenStage++;
			break;

		case 1: // wait for button press
			if ((newButton) && (buttons == EVENT_ENTER_BUTTON_PUSHED))
			{
				ovenStage++;
			}
			break;

		case 2: // 100%
			printProfile(); // Send the profile we are about to run to the usb port
			lcd_gotoxy(0, 1);
			lcd_puts_P("100%           ");
			count = 0;
			EMR_ON; //Turn on the EMR
			_delay_ms(25);
			set_duty_cycle(20); //Turn on the SSR at 100%
			ovenStage++;
			break;

		case 3: // 50%
			if ((ovenTemp >> 2) > 140)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("50%   ");
				set_duty_cycle(10); //Turn on the SSR at 50%
				ovenStage++;
			}
			break;

		case 4: // 10%
			if ((ovenTemp >> 2) > 165)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("10%   ");
				set_duty_cycle(2); //Turn on the SSR at 10%
				ovenCounter = 0;
				ovenStage++;
			}
			break;

		case 5: // wait 45 seconds
		  if (tick)
		  {
				if (ovenCounter < 90)
				{
					ovenCounter++;
				}
				else
				{
					ovenStage++;
				}
				if ((ovenTemp >> 2) > 175)
				{
					lcd_puts_P("0%   ");
					set_duty_cycle(0);
				}					
		  }				
			break;

		case 6: // Maintain 120c
			if (tick)
			{
				lcd_gotoxy(0, 1);
				if ((ovenTemp >> 2) >= 181)
				{
					lcd_puts_P("0%   ");
					set_duty_cycle(0); //Turn off the SSR
				}					
				else if ((ovenTemp >> 2) >= 180)
				{
					lcd_puts_P("10%   ");
					set_duty_cycle(2); //Turn on the SSR at 10%
				}
				else
				{
					lcd_puts_P("20%   ");
					set_duty_cycle(4); //Turn on the SSR at 20%
				}
				if (count == 1200)
				{
					ovenStage++;
				}
			}				
			break;

		case 7:
			fprintf(&USBSerialStream, "=END\n");
			set_duty_cycle(0); //Turn off the SSR
			_delay_ms(25);
			EMR_OFF;
			isRunning = false;
			ovenStage = 0;
			SetIdleMode();
			break;
	}			
};
*/

/*
void CalibrateOvenHandler()
{
	switch (ovenStage)
	{
		case 0: // close door & start message
			lcd_gotoxy(0, 1);
			lcd_puts_P("Close door     ");
			ovenStage++;
			break;

		case 1: // wait for button press
			if ((newButton) && (buttons == EVENT_ENTER_BUTTON_PUSHED))
			{
				ovenStage++;
			}
			break;

		case 2: // 10%
			fprintf (&USBSerialStream, "=OCAL\n");
			lcd_gotoxy(0, 1);
			lcd_puts_P("10%            ");
			count = 0;
			EMR_ON; //Turn on the EMR
			_delay_ms(25);
			set_duty_cycle(2); //Turn on the SSR at 10%
			ovenStage++;
			break;

		case 3: // 20%
			if (count == 480)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("20%   ");
				count = 0;
				set_duty_cycle(4); //Turn on the SSR at 20%
				ovenStage++;
			}
			break;

		case 4: // 30%
			if (count == 480)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("30%   ");
				count = 0;
				set_duty_cycle(6); //Turn on the SSR at 30%
				ovenStage++;
			}
			break;

		case 5: // 40%
			if (count == 480)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("40%   ");
				count = 0;
				set_duty_cycle(8); //Turn on the SSR at 40%
				ovenStage++;
			}
			break;

		case 6: // 50%
			if (count == 480)
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("50%   ");
				count = 0;
				set_duty_cycle(10); //Turn on the SSR at 50%
				ovenStage++;
			}
			break;

		case 7:
			if (count == 480)
			{
				fprintf(&USBSerialStream, "=END\n");
				set_duty_cycle(0); //Turn off the SSR
				_delay_ms(25);
				EMR_OFF;
				isRunning = false;
				ovenStage = 0;
				SetIdleMode();
			}				
			break;
	}			
};
*/
/*
void CalibrateOvenHandler()
{
	switch (ovenStage)
	{
		case 0: // close door & start message
			lcd_gotoxy(0, 1);
			lcd_puts_P("Close door     ");
			ovenStage++;
			break;

		case 1: // wait for button press
			if ((newButton) && (buttons == EVENT_ENTER_BUTTON_PUSHED))
			{
				ovenStage++;
			}
			break;

		case 2: // 100%
			fprintf (&USBSerialStream, "=OCAL\n");
			lcd_gotoxy(0, 1);
			lcd_puts_P("100%           ");
			count = 0;
			EMR_ON; //Turn on the EMR
			_delay_ms(25);
			set_duty_cycle(20); //Turn on the SSR at 100%
			ovenStage++;
			break;

		case 3:
			if ((ovenTemp >> 2) >= 250)
			{
				set_duty_cycle(0); //Turn off the SSR
				_delay_ms(25);
				EMR_OFF;
				ovenStage++;
				ovenCounter = 0;
			}				
			break;
			
		case 4:
		  if (tick)
		  {
				if (ovenCounter < 120)
				{
					ovenCounter++;
				}
				else
				{
					fprintf(&USBSerialStream, "=END\n");
					isRunning = false;
					ovenStage = 0;
					SetIdleMode();
				}
		  }				
			break;
	}			
};
*/

//==============================================================================================================================
//

uint8_t OCALStageDone(void)
{
	if ((ovenDelta64 == 0) && (count >= 600))
	{
		deltaCount++;
		if ((deltaCount == 10) && (!endSet))
		{
			endCount = (count >= 1200) ? count+600 : 1800;
			endSet = 1;
		}
	}
	else
	{
		deltaCount = 0;
	}
	
	if (count == endCount)
	{
		endCount = 3600;
		endSet = 0;
		deltaCount = 0;
		return 1;
	}
	
	return 0;
}

//==============================================================================================================================
//

void OCALIsDone(void)
{
	if (ovenTemp > 1000)
	{
		lcd_gotoxy(0, 1);
		lcd_puts_P("      ");
		count = 0;
		fprintf(&USBSerialStream, "=END\n");
		set_duty_cycle(0); //Turn off the SSR
		_delay_ms(25);
		EMR_OFF;
		isRunning = false;
		ovenStage = 0;
		endCount = 3600;
		endSet = 0;
		SetIdleMode();
	}
	else
	{
		set_duty_cycle(20); //Turn on the SSR at 100%
		ovenStage++;
	}

}

//==============================================================================================================================
//

void CalibrateOvenHandler()
{
	if (ovenTemp > 1080)
	{
		fprintf(&USBSerialStream, "=END\n");
		set_duty_cycle(0); //Turn off the SSR
		_delay_ms(25);
		EMR_OFF;
		isRunning = false;
		ovenStage = 0;
		endCount = 3600;
		endSet = 0;
		SetIdleMode();
		return;
	}
	
	switch (ovenStage)
	{
		case 0: // close door & start message
			lcd_gotoxy(0, 0);
			lcd_puts_P("Oven Cal ");
			lcd_gotoxy(0, 1);
			lcd_puts_P("Close door     ");
			ovenStage++;
			break;

		case 1: // wait for button press
			if ((newButton) && (buttons == EVENT_ENTER_BUTTON_PUSHED))
			{
				ovenStage++;
			}
			break;

		case 2: // start 5%
			fprintf (&USBSerialStream, "=OCAL\n");
			lcd_gotoxy(0, 1);
			lcd_puts_P("5%            ");
			count = 0;
			EMR_ON; //Turn on the EMR
			_delay_ms(25);
			set_duty_cycle(20); //Turn on the SSR at 100%
			endCount = 3600;
			endSet = 0;
			ovenStage++;
			break;

		case 3: // to 5%
			if (count == 6)
			{
				set_duty_cycle(1); //Turn on the SSR at 5%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("10%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 4: // to 10%
			if (count == 50)
			{
				set_duty_cycle(2); //Turn on the SSR at 10%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("15%   ");
				count = 0;
				OCALIsDone();
			}
			break;
			
		case 5: // to 15%
			if (count == 45)
			{
				set_duty_cycle(3); //Turn on the SSR at 15%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("20%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 6: // to 20%
			if (count == 40)
			{
				set_duty_cycle(4); //Turn on the SSR at 20%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("25%   ");
				count = 0;
				OCALIsDone();
			}
			break;
			
		case 7: // to 25%
			if (count == 40)
			{
				set_duty_cycle(5); //Turn on the SSR at 25%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("30%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 8: // to 30%
			if (count == 40)
			{
				set_duty_cycle(6); //Turn on the SSR at 30%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("35%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 9: // to 35%
			if (count == 40)
			{
				set_duty_cycle(7); //Turn on the SSR at 35%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("40%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 10: // to 40%
			if (count == 40)
			{
				set_duty_cycle(8); //Turn on the SSR at 40%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("45%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 11: // to 45%
			if (count == 40)
			{
				set_duty_cycle(9); //Turn on the SSR at 45%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("50%   ");
				count = 0;
				OCALIsDone();
			}
			break;

		case 12: // to 50%
			if (count == 40)
			{
				set_duty_cycle(10); //Turn on the SSR at 50%
			}
			if (OCALStageDone())
			{
				lcd_gotoxy(0, 1);
				lcd_puts_P("      ");
				count = 0;
				fprintf(&USBSerialStream, "=END\n");
				set_duty_cycle(0); //Turn off the SSR
				_delay_ms(25);
				EMR_OFF;
				isRunning = false;
				ovenStage = 0;
				endCount = 3600;
				endSet = 0;
				SetIdleMode();
			}
			break;
	}			
};

//==============================================================================================================================
//
	
void CalibrateProfileCommand()
{
};

//==============================================================================================================================
// Produce a short beep

void keybeep (void)
{
	PORTD |= _BV(7);
	_delay_ms (10);
	PORTD &= ~_BV(7);
}

//==============================================================================================================================
// Process a packet from the PC application

void ProcessPacket(char* packet)
{
	if (strcmp(packet, "**BOOT") == 0) // Command to enter the bootloader
	{
		Bootloader();
	}
	else if (strcmp(packet, "**OGET") == 0) // Command to get oven parameters
	{
		SendOvenSettings();
	}
	else if (strcmp(packet, "**PGET=") == 0) // Command to get oven parameters
	{
	}
	else if (strcmp(packet, "**OCAL") == 0) // Command to calibrate oven
	{
	}
	else if (strcmp(packet, "**PCAL=") == 0) // Command to calibrate profile
	{
	}
}

//==============================================================================================================================
// Read the front panel buttons

uint8_t ReadButtons()
{
	uint8_t tmpb;
		
	tmpb = (~PINC >> 4) & 0x0F;
		
	if (tmpb == buttons)
	{
		return 0;
	}
  buttons = tmpb;
  if (buttons)
  {
		keybeep();
  }		
	return buttons;
}

//==============================================================================================================================
// Set the code into idle mode

void SetIdleMode(void)
{	//
	// set the title bar label and blank the DisplaySpace
	//
	lcd_clrscr();
	lcd_puts_P("Idle");
	showTemp = true;
	//
	// set the button bar and the event handler
	//
	MenuSetEventHandler(IdleDisplayEventHandler);
}

//==============================================================================================================================
// Event handler for idle mode

void IdleDisplayEventHandler(uint8_t Event)
{
	if (Event == EVENT_MENU_BUTTON_PUSHED)
	{	
		showTemp = false;
		MenuDisplay(MainMenu, 1);
	}
}

//==============================================================================================================================
// The main program entry point

int main(void)
{
	SetupHardware();
	
	// Create a regular character stream for the interface so that it can be used with the stdio.h functions
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	sei();

	if (lcdPresent)
	{
	  lcd_clrscr(); // clear display and home cursor
		lcd_puts_P("Reflow Oven USB\nv2."buildstr);
	}

	for (;;)
	{
		if (tick == 6)
		{
			if (lcdPresent)
			{
				lcd_clrscr(); // clear display and home cursor
			}
			// Check oven is calibrated
			if (!eeprom_read_byte(&OvenCalibrated))
			{
				if (lcdPresent)
				{
					lcd_puts_P("Oven\nNot calibrated");
				}
			}
			else // read profiles
			{
				sprintf_P(tmpStr, PSTR("Profiles %02u/%02u"), 0, eeprom_read_byte(&ProfileCount));
				if (lcdPresent)
				{
					lcd_puts_P("Oven Calibrated");
					lcd_gotoxy(0, 1);
					lcd_puts(tmpStr);
				}
				tick++;
			}
		}
		else if (tick == 13)		
		{
			if (lcdPresent)
			{
			  lcd_clrscr(); // clear display and home cursor
			}
			tick = 0;
			break;
		}
		
		if (GetPacket(inBuf, sizeof(inBuf))) // Check if there is incoming USB data
		{
			ProcessPacket(inBuf);
		}
		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}

	// Load the default profile
	eeprom_read_block((void*)&profile, (const void*)&Profiles[0], sizeof(__profile));
	
	SetIdleMode();

	for (;;)
	{
		if ((newButton = ReadButtons()))
		{
			if ((buttons == EVENT_MENU_BUTTON_PUSHED) && (isRunning)) // Turn the oven off and go back to idle
			{ 
				set_duty_cycle(0); //Turn off the SSR
				_delay_ms(25);
				EMR_OFF; //Turn off the EMR
				fprintf(&USBSerialStream, "=ABORT\n");
				isRunning = false;
				ovenStage = 0;
				SetIdleMode();
			}
			else if ((buttons != 0) && (!isRunning) && (lcdPresent)) // Only allow the use of menus if there is an LCD
			{
				MenuExecuteEvent(buttons);
			}
		}

		if (tick)
		{
			UpdateTemp();
		}

		if (isRunning)
		{
			(*ProcessHandler)();
		}
		
		if (tick)
		{
			tick = 0;
		}

		if (GetPacket(inBuf, sizeof(inBuf))) // Check if there is incoming USB data
		{
			ProcessPacket(inBuf);
		}

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
		_delay_ms(10);
	}
}

//==============================================================================================================================
