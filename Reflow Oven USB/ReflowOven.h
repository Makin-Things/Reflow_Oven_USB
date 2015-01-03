//==============================================================================================================================
// U S B   R E F L O W   O V E N   C O N T R O L L E R
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Reflow.h"
// Title 			: USB Reflow Oven Controller
// Date 			: 18 Jul 2011
// Version 		: 1.00
// Target MCU : ATMEGA32U2
// Author			: Simon Ratcliffe


#ifndef _REFLOWOVEN_H_
#define _REFLOWOVEN_H_

//==============================================================================================================================
// Defines

#define MAX_PROFILES			16
#define PROFILE_NAME_LEN	17

//==============================================================================================================================
// Function Prototypes

	void MenuGetEEMEMItem(uint8_t);
	uint8_t GetPacket(char*, uint8_t);
	void SetupHardware(void);
	void UpdateTemp(void);
	void SendOvenSettings(void);
	void Bootloader(void);
	void set_duty_cycle (uint8_t);
	void printProfile (void);
	void RunProfileCommand(void);
	void RunProfileHandler(void);
	void SelectProfileCommand(void);
	void EditProfileCommand(void);
	void CalibrateOvenCommand(void);
	void CalibrateOvenHandler(void);
	void CalibrateProfileCommand(void);
	void Calibrate120cCommand(void);
	void Calibrate120cHandler(void);
	void keybeep (void);
	void ProcessPacket(char*);
	uint8_t ReadButtons(void);
	void EVENT_USB_Device_Connect(void);
	void EVENT_USB_Device_Disconnect(void);
	void EVENT_USB_Device_ConfigurationChanged(void);
	void EVENT_USB_Device_ControlRequest(void);
	void Bootloader(void);
	void SetIdleMode(void);
	void IdleDisplayEventHandler(uint8_t);

#endif

