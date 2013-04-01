//==============================================================================================================================
// S P I   U T I L I T Y   F U N C T I O N S
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Spi.c"
// Title 			: SPI Utility Functions
// Date 			: 18 Jul 2011
// Version 		: 1.00
// Author			: Simon Ratcliffe


//==============================================================================================================================
// Includes

#include <avr/io.h>
#include <stdio.h>

#include "spi.h"

//==============================================================================================================================
// Defines

//#define MAX6675
#define MAX31855

#if defined(MAX6675) && defined(MAX31855)
#error Only one of MAX6675 or MAX31855 can be defined
#endif
#if !defined(MAX6675) && !defined(MAX31855)
#error One of MAX6675 or MAX31855 must be defined
#endif

// SPI port
#define SPI_PORT				PORTB
#define SPI_DDR					DDRB
#define SPI_SS					PB0
#define SPI_SCK					PB1
#define SPI_MOSI				PB2
#define SPI_MISO				PB3

//==============================================================================================================================
// Functions

void spi_init (void)
{
	// setup SPI I/O pins
	SPI_PORT |= _BV(SPI_SCK);	// set SCK hi
	SPI_DDR |= _BV(SPI_SCK);	// set SCK as output
	SPI_DDR &= ~_BV(SPI_MISO);	// set MISO as input
	SPI_DDR |= _BV(SPI_MOSI);	// set MOSI as output
	SPI_DDR |= _BV(SPI_SS);		// SS must be output for Master mode to work
	SPI_PORT |= _BV(SPI_SS); //Set SS high
	// initialize SPI interface
	// master mode
	SPCR |= _BV (MSTR);
	// select clock phase positive-going in middle of data
	SPCR &= ~_BV (CPOL);
	// Data order MSB first
	SPCR &= ~_BV (DORD);
	// switch to f/4 2X = f/2 bitrate
	SPCR &= ~_BV (SPR0);
	SPCR &= ~_BV (SPR1);
	SPSR |= _BV(SPI2X);
	// enable SPI
	SPCR |= _BV (SPE);
}

//==============================================================================================================================

#ifdef MAX6675
uint16_t spi_read (void)
{
	uint16_t val;

	SPI_PORT &= ~_BV(SPI_SS);

	SPDR = 0;
	while(!(SPSR & (1<<SPIF)));
	val = SPDR << 8;

	SPDR = 0;
	while(!(SPSR & (1<<SPIF)));
	val |= SPDR;

	SPI_PORT |= _BV(SPI_SS);

	if (val & _BV(2))
	{
	  return 65535;
	}
	else
	{
		return (val >> 3);
	}
}
#endif

//==============================================================================================================================

#ifdef MAX31855
uint16_t spi_read (void)
{
	uint8_t val[4];
	uint8_t i;
	
	SPI_PORT &= ~_BV(SPI_SS);

	for (i = 0; i < 4; i++)
	{
		SPDR = 0;
		while(!(SPSR & (1<<SPIF)));
		val[i] = SPDR;
	}
	
	SPI_PORT |= _BV(SPI_SS);

	if (val[3] & _BV(0))
	{
		return 65535;
	}
	else if (val[3] & _BV(1))
	{
		return 65534;
	}
	else if (val[3] & _BV(2))
	{
		return 65533;
	}

	return ((val[0] << 8) | (val[1])) >> 2;
}
#endif

//==============================================================================================================================
