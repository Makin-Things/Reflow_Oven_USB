//==============================================================================================================================
// A T M E G A   L C D   M E N U S
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Menu.c"
// Title 			: LCD Menu System
// Date 			: 29 Aug 2011
// Version 		: 1.00
// Target MCU : ATMEGA32U2
// Author			: Simon Ratcliffe


//==============================================================================================================================
// Includes

#include <avr/io.h>
#include <avr/eeprom.h>

#include "lcd.h"
#include "menu.h"
#include "ReflowOven.h"

//==============================================================================================================================
// External variables

extern uint8_t currentProfile;
extern char profileName[PROFILE_NAME_LEN];
extern uint8_t EEMEM ProfileCount;

//==============================================================================================================================
// Defines

#define MAX_MENU_DEPTH	3

//==============================================================================================================================
// Private variables

static MENU_ITEM *CurrentMenuTable;
uint8_t CurrentMenuItemIdx;
static void(*PointerToCurrentEventHandler)(uint8_t);
static uint8_t MenuLevels[MAX_MENU_DEPTH];
static uint8_t CurrentMenuLevel = 0;

//==============================================================================================================================
// Function Prototypes (Private)

static void MenuShowTitle(PGM_P);
static void MenuShowItem(uint8_t);
static void MenuShowItemEEMEM(uint8_t);
static void MenuDisplayEventHandler(uint8_t Event);
static void MenuChangeSelection(uint8_t);

//==============================================================================================================================
// Display a menu

void MenuDisplay(const MENU_ITEM *Menu, uint8_t MenuItemIdxToSelect)
{
	// Remember this menu and select the first item in the menu
	CurrentMenuTable = (MENU_ITEM *)Menu;
	CurrentMenuItemIdx = MenuItemIdxToSelect;
	
	MenuShowTitle((PGM_P)pgm_read_word(&Menu[0].MenuItemText)); // Display the menu name in the title line

	if (pgm_read_byte(&Menu[1].MenuItemType) != MENU_ITEM_TYPE_EEMEM_COMMAND)
	{
		MenuShowItem(MenuItemIdxToSelect); // Display the chosen menu item
	}
	else
	{
		CurrentMenuItemIdx = currentProfile;
		MenuShowItemEEMEM(CurrentMenuItemIdx); // Display the chosen menu item
	}

	MenuSetEventHandler(MenuDisplayEventHandler); // Set the event handler
}

//==============================================================================================================================
// Set the menu event handler

void MenuSetEventHandler(void (*PntrToNewEventHandler)(uint8_t))
{	
	PointerToCurrentEventHandler = PntrToNewEventHandler;
}

//==============================================================================================================================
// Execute the menu event handler

void MenuExecuteEvent(uint8_t Event)
{	
	(*PointerToCurrentEventHandler)(Event);
}

//==============================================================================================================================
// Display the menu title

static void MenuShowTitle(PGM_P TitleText)
{		
	lcd_gotoxy(0, 0);
	lcd_puts_p(TitleText);
}

//==============================================================================================================================
// Display a menu item

static void MenuShowItem(uint8_t MenuIdx)
{
	lcd_gotoxy(0, 1);
	lcd_puts_p((PGM_P)pgm_read_word(&CurrentMenuTable[MenuIdx].MenuItemText));
}

//==============================================================================================================================
// Display a menu item

static void MenuShowItemEEMEM(uint8_t MenuIdx)
{
	void (*CommandPntr)(uint8_t);

	CommandPntr = (void (*)(uint8_t))pgm_read_word(&CurrentMenuTable[1].MenuItemText);
	if (CommandPntr != 0)
		(*CommandPntr)(MenuIdx); // Get the string from EEMEM

	lcd_gotoxy(0, 1);
	lcd_puts(profileName);
}

//==============================================================================================================================
// Handle menu events

static void MenuDisplayEventHandler(uint8_t Event)
{	
	void (*CommandPntr)(void);
	MENU_ITEM *SubMenuTable;

	switch (Event) // Determine the event type
	{
		case EVENT_UP_BUTTON_PUSHED: // Check if the UP button was pushed
			MenuChangeSelection(CurrentMenuItemIdx - 1);
			break;

		case EVENT_DOWN_BUTTON_PUSHED:  // Check if the DOWN button was pushed
			MenuChangeSelection(CurrentMenuItemIdx + 1);
			break;

		case EVENT_ENTER_BUTTON_PUSHED: // Check if the ENTER button was pushed
			if (pgm_read_byte(&CurrentMenuTable[1].MenuItemType) != MENU_ITEM_TYPE_EEMEM_COMMAND)
			{
				switch (pgm_read_byte(&CurrentMenuTable[CurrentMenuItemIdx].MenuItemType)) // determine the menu item type
				{	
					case MENU_ITEM_TYPE_COMMAND: // Its a command so execute the command function
						CommandPntr = (void (*)(void))pgm_read_word(&CurrentMenuTable[CurrentMenuItemIdx].MenuItemPntr);
						if (CommandPntr != 0)
							(*CommandPntr)(); 
						break;

					case MENU_ITEM_TYPE_SUB_MENU: // Its a PROGMEM sub-menu so show the sub-menu
						MenuLevels[CurrentMenuLevel] = CurrentMenuItemIdx;
						CurrentMenuLevel++;
						SubMenuTable = (MENU_ITEM *)pgm_read_word(&CurrentMenuTable[CurrentMenuItemIdx].MenuItemPntr);
						MenuDisplay(SubMenuTable, 1);
						break;
				}
			}
			else
			{
				CommandPntr = (void (*)(void))pgm_read_word(&CurrentMenuTable[1].MenuItemPntr);
				if (CommandPntr != 0)
					(*CommandPntr)(); 
			}				
			break;

		case EVENT_MENU_BUTTON_PUSHED: // Check if the MENU button was pushed
			switch (pgm_read_byte(&CurrentMenuTable[0].MenuItemType))
			{	
				case MENU_ITEM_TYPE_MAIN_MENU_HEADER: // At the top of the menu so go back to idle mode
					CommandPntr = (void (*)(void))pgm_read_word(&CurrentMenuTable[0].MenuItemPntr);
					if (CommandPntr != 0)
						(*CommandPntr)(); 
					break;

				case MENU_ITEM_TYPE_SUB_MENU_HEADER: // Jump up one level in the menus
					if (pgm_read_word(&CurrentMenuTable[0].MenuItemPntr) != 0)
					{
						CurrentMenuLevel--;
						MenuDisplay((MENU_ITEM*)pgm_read_word(&CurrentMenuTable[0].MenuItemPntr), MenuLevels[CurrentMenuLevel]);
					}					
					break;
			}
			break;
	}
}

//==============================================================================================================================
// Handle the UP/DOWN menu selection

static void MenuChangeSelection(uint8_t NewMenuItemIdx)
{	
	if (pgm_read_byte(&CurrentMenuTable[1].MenuItemType) != MENU_ITEM_TYPE_EEMEM_COMMAND)
	{
		if ((NewMenuItemIdx >= 1) && 
			(pgm_read_byte(&CurrentMenuTable[NewMenuItemIdx].MenuItemType) != MENU_ITEM_TYPE_END_OF_MENU) &&
			(NewMenuItemIdx != CurrentMenuItemIdx))
		{
			CurrentMenuItemIdx = NewMenuItemIdx; // Select the new menu item
		
			MenuShowItem(NewMenuItemIdx); // Show the new menu
		}
	}
	else
	{
		if ((NewMenuItemIdx >= 1) && 
			(NewMenuItemIdx <= eeprom_read_byte(&ProfileCount)) &&
			(NewMenuItemIdx != CurrentMenuItemIdx))
		{
			CurrentMenuItemIdx = NewMenuItemIdx; // Select the new menu item
		
			MenuShowItemEEMEM(NewMenuItemIdx); // Show the new menu
		}
	}	
}

//==============================================================================================================================
