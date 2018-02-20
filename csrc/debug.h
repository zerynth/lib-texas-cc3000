/**************************************************************************/
/*!
  @file     Adafruit_CC3000.cpp
  @author   KTOWN (Kevin Townsend for Adafruit Industries)
	@license  BSD (see license.txt)

	This is a library for the Adafruit CC3000 WiFi breakout board
	This library works with the Adafruit CC3000 breakout
	----> https://www.adafruit.com/products/1469

	Check out the links above for our tutorials and wiring diagrams
	These chips use SPI to communicate.

	Adafruit invests time and resources providing this open source code,
	please support Adafruit and open-source hardware by purchasing
	products from Adafruit!

	@section  HISTORY

	v1.0    - Initial release
*/
/**************************************************************************/


#ifndef _CC3000_DEBUG
#define _CC3000_DEBUG


extern int32_t errno;

#define DEBUG_MODE                      (1)

#if (DEBUG_MODE != 0)

#define printf(...) vbl_printf_stdout(__VA_ARGS__)

#else

#define printf(...)

#endif

#define DEBUGPRINT_F(...) printf(__VA_ARGS__)
#define DEBUGPRINT_DEC(x) printf("%i",x)
#define DEBUGPRINT_HEX16(n) printf("%x",n)
#define DEBUGPRINT_HEX(n) printf("%x",n)

#endif
