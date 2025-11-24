// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file math.h
 * @brief Math library header for ValkyrieOS kernel
 * 
 * Provides common mathematical functions for integer and floating-point arithmetic,
 * trigonometry, exponential/logarithmic operations, and rounding.
 */

#ifndef LIBMATH_MATH_H
#define LIBMATH_MATH_H

/* ===== Math Constants ===== */

#define M_PI        3.14159265358979323846   /**< Pi (π) */
#define M_E         2.71828182845904523536   /**< Euler's number (e) */
#define M_LN2       0.69314718055994530942   /**< Natural log of 2 */
#define M_LN10      2.30258509299404568402   /**< Natural log of 10 */
#define M_SQRT2     1.41421356237309504880   /**< Square root of 2 */

#define INFINITY    __builtin_inf()          /**< Positive infinity */
#define NAN         __builtin_nanf("")       /**< Not-a-number */

/* ===== Integer Arithmetic ===== */

/**
 * Add two integers
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
extern int add(int a, int b) __attribute__((weak));

/**
 * Subtract two integers
 * @param a First number (minuend)
 * @param b Second number (subtrahend)
 * @return Difference a - b
 */
extern int subtract(int a, int b) __attribute__((weak));

/**
 * Multiply two integers
 * @param a First number
 * @param b Second number
 * @return Product of a and b
 */
extern int multiply(int a, int b) __attribute__((weak));

/**
 * Divide two integers
 * @param a Dividend
 * @param b Divisor
 * @return Integer quotient a / b (returns 0 if b == 0)
 */
extern int divide(int a, int b) __attribute__((weak));

/**
 * Modulo operation
 * @param a Dividend
 * @param b Divisor
 * @return Remainder a % b (returns 0 if b == 0)
 */
extern int modulo(int a, int b) __attribute__((weak));

/**
 * Absolute value of integer
 * @param x Value
 * @return Absolute value |x|
 */
extern int abs_int(int x) __attribute__((weak));

/* ===== Floating-Point Absolute Value ===== */

/**
 * Absolute value of float
 * @param x Value
 * @return Absolute value |x|
 */
extern float fabsf(float x) __attribute__((weak));

/**
 * Absolute value of double
 * @param x Value
 * @return Absolute value |x|
 */
extern double fabs(double x) __attribute__((weak));

/* ===== Trigonometric Functions ===== */

/**
 * Sine of x (in radians)
 * @param x Angle in radians
 * @return sin(x)
 */
extern float sinf(float x) __attribute__((weak));

extern double sin(double x) __attribute__((weak));

/**
 * Cosine of x (in radians)
 * @param x Angle in radians
 * @return cos(x)
 */
extern float cosf(float x) __attribute__((weak));

extern double cos(double x) __attribute__((weak));

/**
 * Tangent of x (in radians)
 * @param x Angle in radians
 * @return tan(x)
 */
extern float tanf(float x) __attribute__((weak));

extern double tan(double x) __attribute__((weak));

/* ===== Exponential & Logarithm ===== */

/**
 * Exponential function e^x
 * @param x Exponent
 * @return e^x
 */
extern float expf(float x) __attribute__((weak));

extern double exp(double x) __attribute__((weak));

/**
 * Natural logarithm (base e)
 * @param x Value (must be > 0)
 * @return ln(x)
 */
extern float logf(float x) __attribute__((weak));

extern double log(double x) __attribute__((weak));

/**
 * Base-10 logarithm
 * @param x Value (must be > 0)
 * @return log₁₀(x)
 */
extern float log10f(float x) __attribute__((weak));

extern double log10(double x) __attribute__((weak));

/* ===== Power Function ===== */

/**
 * Power function: x raised to power y
 * @param x Base
 * @param y Exponent
 * @return x^y
 */
extern float powf(float x, float y) __attribute__((weak));

extern double pow(double x, double y) __attribute__((weak));

/**
 * Square root
 * @param x Value (must be >= 0)
 * @return √x
 */
extern float sqrtf(float x) __attribute__((weak));

extern double sqrt(double x) __attribute__((weak));

/* ===== Rounding ===== */

/**
 * Floor: largest integer <= x
 * @param x Value
 * @return ⌊x⌋
 */
extern float floorf(float x) __attribute__((weak));

extern double floor(double x) __attribute__((weak));

/**
 * Ceiling: smallest integer >= x
 * @param x Value
 * @return ⌈x⌉
 */
extern float ceilf(float x) __attribute__((weak));

extern double ceil(double x) __attribute__((weak));

/**
 * Round to nearest integer
 * @param x Value
 * @return Rounded value
 */
extern float roundf(float x) __attribute__((weak));

extern double round(double x) __attribute__((weak));

/* ===== Min/Max ===== */

/**
 * Minimum of two floats
 * @param x First value
 * @param y Second value
 * @return min(x, y)
 */
extern float fminf(float x, float y)__attribute__((weak));

extern double fmin(double x, double y) __attribute__((weak));

/**
 * Maximum of two floats
 * @param x First value
 * @param y Second value
 * @return max(x, y)
 */
extern float fmaxf(float x, float y) __attribute__((weak));

extern double fmax(double x, double y) __attribute__((weak));

/* ===== Floating-Point Modulo ===== */

/**
 * Floating-point remainder
 * @param x Dividend
 * @param y Divisor (must be != 0)
 * @return x - trunc(x/y) * y
 */
extern float fmodf(float x, float y) __attribute__((weak));

extern double fmod(double x, double y) __attribute__((weak));

/* ===== Library Entry Point ===== */

/**
 * Library initialization function
 * Called when libmath is loaded by the dynamic linker
 * @return 0 on success
 */
extern int libmath_init(void) __attribute__((weak));

#endif /* LIBMATH_MATH_H */
