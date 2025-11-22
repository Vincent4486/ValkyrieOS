// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * Example dynamic library for testing library function calls
 * Exports: add(), subtract(), multiply()
 */

/**
 * Add two integers
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
int add(int a, int b)
{
   return a + b;
}

/**
 * Subtract two integers
 * @param a First number
 * @param b Second number
 * @return Difference of a - b
 */
int subtract(int a, int b)
{
   return a - b;
}

/**
 * Multiply two integers
 * @param a First number
 * @param b Second number
 * @return Product of a and b
 */
int multiply(int a, int b)
{
   return a * b;
}

/**
 * Library entry point (called by dylib_call_if_exists)
 * @return Always returns 0
 */
int libmath_init(void)
{
   return 0;
}
