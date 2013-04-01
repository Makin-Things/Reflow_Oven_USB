//==============================================================================================================================
// A T M E G A   L C D   M E N U S
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Menu.h"
// Title 			: LCD Menu System
// Date 			: 29 Aug 2011
// Version 		: 1.00
// Target MCU : ATMEGA32U2
// Author			: Simon Ratcliffe


#ifndef MENU_H_
#define MENU_H_

//==============================================================================================================================
// Typedefs

typedef struct _MENU_ITEM
{	
	uint8_t MenuItemType;
	PGM_P MenuItemText;
	PGM_P MenuItemPntr;
} MENU_ITEM;

//==============================================================================================================================
// Defines

// button codes
#define EVENT_MENU_BUTTON_PUSHED	1
#define EVENT_UP_BUTTON_PUSHED		2
#define EVENT_DOWN_BUTTON_PUSHED	4
#define EVENT_ENTER_BUTTON_PUSHED	8

// menu item types
#define MENU_ITEM_TYPE_MAIN_MENU_HEADER		0
#define MENU_ITEM_TYPE_SUB_MENU_HEADER		1
#define MENU_ITEM_TYPE_SUB_MENU						2
#define MENU_ITEM_TYPE_COMMAND						3
#define MENU_ITEM_TYPE_EEMEM_COMMAND			4
#define MENU_ITEM_TYPE_END_OF_MENU				5

//==============================================================================================================================
// Function Prototypes (Public)
void MenuDisplay(const MENU_ITEM*, uint8_t);
void MenuSetEventHandler(void (*)(uint8_t));
void MenuExecuteEvent(uint8_t);

#endif /* MENU_H_ */