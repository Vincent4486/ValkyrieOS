// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file float.h
 * @brief Floating-point limits and characteristics for ValkyrieOS kernel
 * 
 * Defines limits for float and double types, similar to standard C <float.h>
 */

#ifndef LIBMATH_FLOAT_H
#define LIBMATH_FLOAT_H

/* ===== Float (32-bit) Limits ===== */

#define FLT_RADIX           2           /**< Radix of floating-point representation */
#define FLT_MANT_DIG        24          /**< Number of mantissa digits (float) */
#define FLT_DIG             6           /**< Decimal digits of precision (float) */
#define FLT_MIN_EXP         -125        /**< Minimum binary exponent (float) */
#define FLT_MAX_EXP         128         /**< Maximum binary exponent (float) */
#define FLT_MIN_10_EXP      -37         /**< Minimum decimal exponent (float) */
#define FLT_MAX_10_EXP      38          /**< Maximum decimal exponent (float) */

#define FLT_MAX             3.40282347e+38f   /**< Maximum finite float value */
#define FLT_MIN             1.17549435e-38f   /**< Minimum positive float value */
#define FLT_EPSILON         1.19209290e-07f   /**< Smallest float x where 1.0 + x != 1.0 */

/* ===== Double (64-bit) Limits ===== */

#define DBL_MANT_DIG        53          /**< Number of mantissa digits (double) */
#define DBL_DIG             15          /**< Decimal digits of precision (double) */
#define DBL_MIN_EXP         -1021       /**< Minimum binary exponent (double) */
#define DBL_MAX_EXP         1024        /**< Maximum binary exponent (double) */
#define DBL_MIN_10_EXP      -307        /**< Minimum decimal exponent (double) */
#define DBL_MAX_10_EXP      308         /**< Maximum decimal exponent (double) */

#define DBL_MAX             1.7976931348623157e+308   /**< Maximum finite double value */
#define DBL_MIN             2.2250738585072014e-308   /**< Minimum positive double value */
#define DBL_EPSILON         2.2204460492503131e-16    /**< Smallest double x where 1.0 + x != 1.0 */

/* ===== Long Double (extended precision) ===== */

#define LDBL_MANT_DIG       64         /**< Number of mantissa digits (long double) */
#define LDBL_DIG            18         /**< Decimal digits of precision (long double) */
#define LDBL_MIN_EXP        -16381     /**< Minimum binary exponent (long double) */
#define LDBL_MAX_EXP        16384      /**< Maximum binary exponent (long double) */
#define LDBL_MIN_10_EXP     -4931      /**< Minimum decimal exponent (long double) */
#define LDBL_MAX_10_EXP     4932       /**< Maximum decimal exponent (long double) */

#define LDBL_MAX            1.1897314953572317646e+4932L   /**< Maximum finite long double */
#define LDBL_MIN            3.3621031431120935063e-4932L   /**< Minimum positive long double */
#define LDBL_EPSILON        1.0842021724999998051e-19L     /**< Smallest long double x where 1.0 + x != 1.0 */

/* ===== Rounding Mode ===== */

#define FLT_ROUNDS          1           /**< Rounding mode: 1 = round to nearest */

/* ===== Floating-Point Classification ===== */

#define FP_INFINITE         1           /**< Classification: infinite */
#define FP_NAN              2           /**< Classification: not-a-number */
#define FP_NORMAL           3           /**< Classification: normal */
#define FP_SUBNORMAL        4           /**< Classification: subnormal */
#define FP_ZERO             5           /**< Classification: zero */

#endif /* LIBMATH_FLOAT_H */
