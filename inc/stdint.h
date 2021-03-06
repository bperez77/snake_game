/* stdint.h
 *
 * ECE348, Spring 2015
 * Group C4
 * Brandon Perez bmperez
 * Rohit Banerjee rohitban
 *
 * Target: MC9S12C128
 * CodeWarrior Version: 5.1
 *
 * Created: Fri 24 Apr 2015 05:00:24 PM EDT
 * Last Modified: Thu 07 May 2015 08:42:40 PM EDT
 *
 * This file contains the equivalent version of the C standard library's
 * header file stdint.h. Contains definitions for sized integers.
 */

#ifndef STDINT_H_
#define STDINT_H_

// Defintions for signed types
typedef char int8_t;
typedef int int16_t;
typedef long int32_t;

// Definitions for unsigned types
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;

#endif /* STDINT_H_ */
