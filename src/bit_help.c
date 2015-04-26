/* bit_help.c
 *
 * ECE348, Spring 2015
 * Group C4
 * Brandon Perez bmperez
 * Rohit Banerjee rohitban
 *
 * Target: MC9S12C128
 * CodeWarrior Version: 5.1
 *
 * Created: Sun 26 Apr 2015 05:55:01 PM EDT
 * Last Modified: TODO: Update
 *
 * This file contains various helper functions for performing bit-wise
 * operations on uint8_t elements, and a modulo function for powers of 2.
 */

#include "stdint.h"

/* mask_below
 *
 * Generates a mask that masks out the bits below and including bit_pos,
 * [bit_pos:0], in an 8-bit number. Bit_pos must be between 7 and 0.
 */
uint8_t mask_below(int8_t bit_pos)
{
    return (uint8_t)((((uint16_t)1) << (bit_pos + 1)) - 1);
}

/* mask_above
 *
 * Generates a mask that masks out the bits above and including bit_pos,
 * [7:bit_pos], in an 8-bit number. Bit_pos must be between 7 and 0.
 */
uint8_t mask_above(int8_t bit_pos)
{
    return (uint8_t)(-(((uint16_t)1) << bit_pos));
}

/* mask_between
 *
 * Generates a mask that that masks out the bits between lower and upper,
 * inclusive [upper:lower], in an 8-bit number. Lower and upper must be
 * between 7 and 0.
 */
uint8_t mask_between(int8_t lower, int8_t upper)
{
    return mask_above(lower) & mask_below(upper);
}

/* set_bits
 *
 * Sets the bits from position lower to upper, x[upper:lower], with the lower
 * (upper - lower) bits of y. The rest of the bits of x are preserved. If
 * any other bits of y are set, then the result is undefined. Lower and upper
 * must both be between 7 and 0.
 */
uint8_t set_bits(uint8_t x, uint8_t y, int8_t lower, int8_t upper)
{
    return (x & ~mask_between(lower,upper)) |
           ((y & mask_between(0, upper-lower)) << lower);
}

/* mod
 *
 * Computes x % limit. Limit must be a power of 2. The modulo computed
 * is strictly non-negative.
 */
int mod(int x, int limit)
{
    return x & (limit - 1);
}