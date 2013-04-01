//==============================================================================================================================
// U S A R T   U T I L I T Y   F U N C T I O N S
//
// Copyright	: 2011 ProAtomic Software Development Pty Ltd
// File Name	: "Usart.h"
// Title 			: USART Utility Functions
// Date 			: 18 Jul 2011
// Version 		: 1.00
// Author			: Simon Ratcliffe


#ifndef USART_H_
#define USART_H_

//	extern FILE mystdout;

	void usart_init (void);
	int usart_putchar (char c, FILE* stream);

#endif /* USART_H_ */