/*
 * debugmacros.h
 *
 *  Created on: May 9, 2017
 *      Author: DK
 */

#ifndef DEBUGMACROS_H_
#define DEBUGMACROS_H_

#define DEBUG
#define INFO
#ifdef DEBUG
	#define DEBUG_PRINT(x) Serial.print(x)
	#define DEBUG_PRINTLN(x) Serial.println(x)
	#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
	#define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
 #else
	#define DEBUG_PRINT(x)
	#define DEBUG_PRINTLN(x)
	#define DEBUG_PRINTF(...)
	#define DEBUG_PRINTDEC(x)
 #endif

#ifdef INFO
	#define INFO_PRINT(x) Serial.print(x)
	#define INFO_PRINTLN(x) Serial.println(x)
	#define INFO_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
	#define INFO_PRINT(x)
	#define INFO_PRINTLN(x)
	#define INFO_PRINTF(...)
#endif

#endif /* DEBUGMACROS_H_ */
