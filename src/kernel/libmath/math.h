// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * libmath - Math library for dynamic linking
 * Provides basic arithmetic functions
 */

#ifndef LIBMATH_H
#define LIBMATH_H

/**
 * Add two integers
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
extern int add(int a, int b);

/**
 * Subtract two integers
 * @param a First number
 * @param b Second number
 * @return Difference of a - b
 */
extern int subtract(int a, int b);

/**
 * Multiply two integers
 * @param a First number
 * @param b Second number
 * @return Product of a and b
 */
extern int multiply(int a, int b);

/**
 * Library entry point (called by dylib_call_if_exists)
 * @return Always returns 0
 */
extern int libmath_init(void);

#endif // LIBMATH_H
