/*
  HardwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 23 November 2006 by David A. Mellis
  Modified 28 September 2010 by Mark Sproul
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "core_build_options.h"
#include "wiring.h"
#include "wiring_private.h"

// this next line disables the entire HardwareSerial.cpp,
// this is so I can support Attiny series and any other chip without a uart
// (If DISABLE_UART is set in core_build_options.h, HW serial is disabled completely.)
#if defined(UBRRH) || defined(UBRR0H) || defined(UBRR1H) || defined(UBRR2H) || defined(UBRR3H) && !DISABLE_UART

#include "HardwareSerial.h"

// Define constants and variables for buffering incoming serial data.  We're
// using a ring buffer (I think), in which rx_buffer_head is the index of the
// location to which to write the next incoming character and rx_buffer_tail
// is the index of the location from which to read.
#if (RAMEND < 1000)
  #define RX_BUFFER_SIZE 32
#else
  #define RX_BUFFER_SIZE 64
#endif

struct ring_buffer
{
  unsigned char buffer[RX_BUFFER_SIZE];
  int head;
  int tail;
};

#if defined(UBRRH) || defined(UBRR0H)
  ring_buffer rx_buffer  =  { { 0 }, 0, 0 };
#endif
#if defined(UBRR1H)
  ring_buffer rx_buffer1  =  { { 0 }, 0, 0 };
#endif
#if defined(UBRR2H)
  ring_buffer rx_buffer2  =  { { 0 }, 0, 0 };
#endif
#if defined(UBRR3H)
  ring_buffer rx_buffer3  =  { { 0 }, 0, 0 };
#endif

inline void store_char(unsigned char c, ring_buffer *rx_buffer)
{
  int i = (unsigned int)(rx_buffer->head + 1) % RX_BUFFER_SIZE;

  // if we should be storing the received character into the location
  // just before the tail (meaning that the head would advance to the
  // current location of the tail), we're about to overflow the buffer
  // and so we don't write the character or advance the head.
  if (i != rx_buffer->tail) {
    rx_buffer->buffer[rx_buffer->head] = c;
    rx_buffer->head = i;
  }
}

#if defined(USART_RX_vect)
  ISR(USART_RX_vect)
  {
  #if defined(UDR0)
    unsigned char c  =  UDR0;
  #elif defined(UDR)
    unsigned char c  =  UDR;  //  atmega8535
  #else
    #error UDR not defined
  #endif
    store_char(c, &rx_buffer);
  }
#elif defined(USART0_RECV_vect) && defined(UDR0)
  ISR(USART0_RECV_vect)
  {
    unsigned char c  =  UDR0;
    store_char(c, &rx_buffer);
  }
#elif defined(UART0_RECV_vect) && defined(UDR0)
  ISR(UART0_RECV_vect)
  {
    unsigned char c  =  UDR0;
    store_char(c, &rx_buffer);
  }
//#elif defined(SIG_USART_RECV)
#elif defined(USART0_RX_vect)
  // fixed by Mark Sproul this is on the 644/644p
  //ISR(SIG_USART_RECV)
  ISR(USART0_RX_vect)
  {
  #if defined(UDR0)
    unsigned char c  =  UDR0;
  #elif defined(UDR)
    unsigned char c  =  UDR;  //  atmega8, atmega32
  #else
    #error UDR not defined
  #endif
    store_char(c, &rx_buffer);
  }
#elif defined(UART_RECV_vect)
  // this is for atmega8
  ISR(UART_RECV_vect)
  {
  #if defined(UDR0)
    unsigned char c  =  UDR0;  //  atmega645
  #elif defined(UDR)
    unsigned char c  =  UDR;  //  atmega8
  #endif
    store_char(c, &rx_buffer);
  }
#elif defined(USBCON)
  #warning No interrupt handler for usart 0
  #warning Serial(0) is on USB interface
#else
  #error No interrupt handler for usart 0
#endif

//#if defined(SIG_USART1_RECV)
#if defined(USART1_RX_vect)
  //ISR(SIG_USART1_RECV)
  ISR(USART1_RX_vect)
  {
    unsigned char c = UDR1;
    store_char(c, &rx_buffer1);
  }
#endif

#if defined(USART2_RX_vect) && defined(UDR2)
  ISR(USART2_RX_vect)
  {
    unsigned char c = UDR2;
    store_char(c, &rx_buffer2);
  }
#endif

#if defined(USART3_RX_vect) && defined(UDR3)
  ISR(USART3_RX_vect)
  {
    unsigned char c = UDR3;
    store_char(c, &rx_buffer3);
  }
#endif



// Constructors ////////////////////////////////////////////////////////////////

HardwareSerial::HardwareSerial(ring_buffer *rx_buffer,
  volatile uint8_t *ubrrh, volatile uint8_t *ubrrl,
  volatile uint8_t *ucsra, volatile uint8_t *ucsrb,  volatile uint8_t *ucsrc,
  volatile uint8_t *udr,
  uint8_t rxen, uint8_t txen, uint8_t rxcie, uint8_t udre, uint8_t u2x)
{
  _rx_buffer = rx_buffer;
  _ubrrh = ubrrh;
  _ubrrl = ubrrl;
  _ucsra = ucsra;
  _ucsrb = ucsrb;
  _ucsrc = ucsrc;
  _udr = udr;
  _rxen = rxen;
  _txen = txen;
  _rxcie = rxcie;
  _udre = udre;
  _u2x = u2x;
}

// Public Methods //////////////////////////////////////////////////////////////

void HardwareSerial::begin(unsigned long baud, byte config)
{
  uint16_t baud_setting;
  bool use_u2x = true;

#if F_CPU == 16000000UL
  // hardcoded exception for compatibility with the bootloader shipped
  // with the Duemilanove and previous boards and the firmware on the 8U2
  // on the Uno and Mega 2560.
  if (baud == 57600) {
    use_u2x = false;
  }
#endif
  
  if (use_u2x) {
    *_ucsra = 1 << _u2x;
    baud_setting = (F_CPU / 4 / baud - 1) / 2;
  } else {
    *_ucsra = 0;
    baud_setting = (F_CPU / 8 / baud - 1) / 2;
  }

  // assign the baud_setting, a.k.a. ubbr (USART Baud Rate Register)
  *_ubrrh = baud_setting >> 8;
  *_ubrrl = baud_setting;

  *_ucsrc = config;
  sbi(*_ucsrb, _rxen);
  sbi(*_ucsrb, _txen);
  sbi(*_ucsrb, _rxcie);
}

void HardwareSerial::end()
{
  cbi(*_ucsrb, _rxen);
  cbi(*_ucsrb, _txen);
  cbi(*_ucsrb, _rxcie);  
}

int HardwareSerial::available(void)
{
  return (unsigned int)(RX_BUFFER_SIZE + _rx_buffer->head - _rx_buffer->tail) % RX_BUFFER_SIZE;
}

int HardwareSerial::peek(void)
{
  if (_rx_buffer->head == _rx_buffer->tail) {
    return -1;
  } else {
    return _rx_buffer->buffer[_rx_buffer->tail];
  }
}

int HardwareSerial::read(void)
{
  // if the head isn't ahead of the tail, we don't have any characters
  if (_rx_buffer->head == _rx_buffer->tail) {
    return -1;
  } else {
    unsigned char c = _rx_buffer->buffer[_rx_buffer->tail];
    _rx_buffer->tail = (unsigned int)(_rx_buffer->tail + 1) % RX_BUFFER_SIZE;
    return c;
  }
}

void HardwareSerial::flush()
{
  // don't reverse this or there may be problems if the RX interrupt
  // occurs after reading the value of rx_buffer_head but before writing
  // the value to rx_buffer_tail; the previous value of rx_buffer_head
  // may be written to rx_buffer_tail, making it appear as if the buffer
  // don't reverse this or there may be problems if the RX interrupt
  // occurs after reading the value of rx_buffer_head but before writing
  // the value to rx_buffer_tail; the previous value of rx_buffer_head
  // may be written to rx_buffer_tail, making it appear as if the buffer
  // were full, not empty.
  _rx_buffer->head = _rx_buffer->tail;
}

size_t HardwareSerial::write(uint8_t c)
{
  while (!((*_ucsra) & (1 << _udre)))
    ;

  *_udr = c;

  return( 1 );
}

// Preinstantiate Objects //////////////////////////////////////////////////////

#if ! DEFAULT_TO_TINY_DEBUG_SERIAL
  #if defined(UBRRH) && defined(UBRRL)
    HardwareSerial Serial(&rx_buffer, &UBRRH, &UBRRL, &UCSRA, &UCSRB, &UCSRC, &UDR, RXEN, TXEN, RXCIE, UDRE, U2X);
  #elif defined(UBRR0H) && defined(UBRR0L)
    HardwareSerial Serial(&rx_buffer, &UBRR0H, &UBRR0L, &UCSR0A, &UCSR0B, &UCSR0C, &UDR0, RXEN0, TXEN0, RXCIE0, UDRE0, U2X0);
  #elif defined(USBCON)
    #warning no serial port defined  (port 0)
  #else
    #error no serial port defined  (port 0)
  #endif
#endif

#if defined(UBRR1H)
  HardwareSerial Serial1(&rx_buffer1, &UBRR1H, &UBRR1L, &UCSR1A, &UCSR1B, &UCSR1C, &UDR1, RXEN1, TXEN1, RXCIE1, UDRE1, U2X1);
#endif
#if defined(UBRR2H)
  HardwareSerial Serial2(&rx_buffer2, &UBRR2H, &UBRR2L, &UCSR2A, &UCSR2B, &UCSR2C, &UDR2, RXEN2, TXEN2, RXCIE2, UDRE2, U2X2);
#endif
#if defined(UBRR3H)
  HardwareSerial Serial3(&rx_buffer3, &UBRR3H, &UBRR3L, &UCSR3A, &UCSR3B, &UCSR3C, &UDR3, RXEN3, TXEN3, RXCIE3, UDRE3, U2X3);
#endif

#endif // whole file
