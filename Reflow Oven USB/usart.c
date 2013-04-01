//==============================================================================================================================
// U S A R T   U T I L I T Y   F U N C T I O N S
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Usart.c"
// Title 			: USART Utility Functions
// Date 			: 18 Jul 2011
// Version 		: 1.00
// Author			: Simon Ratcliffe


//==============================================================================================================================
// Includes

#include <avr/io.h>
#include <stdio.h>

#include "usart.h"

//==============================================================================================================================
// Global Variables

static FILE mystdout = FDEV_SETUP_STREAM(usart_putchar, NULL,_FDEV_SETUP_WRITE);

//==============================================================================================================================
// Functions

void usart_init (void)
{
#define BAUD 9600
#include <util/setbaud.h>
	UBRR1H = UBRRH_VALUE;
	UBRR1L = UBRRL_VALUE;
#if USE_2X
	UCSR1A |= (1 << U2X1);
#else
	UCSR1A &= ~(1 << U2X1);
#endif
	UCSR1B = (1 << TXEN1);
	UCSR1C = (3 << UCSZ10);
	
	stdout = &mystdout;
}

//==============================================================================================================================

int usart_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		usart_putchar('\r', stream);
	}
	loop_until_bit_is_set(UCSR1A, UDRE1);
	UDR1 = c;
	return 0;
}

//==============================================================================================================================
