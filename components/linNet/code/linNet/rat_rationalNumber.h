#ifndef RAT_RATIONALNUMBER_INCLUDED
#define RAT_RATIONALNUMBER_INCLUDED
/**
 * @file rat_rationalNumber.h
 * Definition of global interface of module rat_rationalNumber.c
 *
 * Copyright (C) 2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Include files
 */

#include <limits.h>
#include "types.h"
#include "log_logger.h"


/*
 * Defines
 */

/** Normally, we operate (internally) with the longest available integers, 64 Bit.
    Numerator and denominator are long int, 32 Bit. For testing purpose it may be easier to
    temporarily set this switch to false. Then the numerator and denominator have only 16
    Bit and the operations are done with 32 Bit. */
#define RAT_USE_64BIT_ARITHMETIC    1


#if RAT_USE_64BIT_ARITHMETIC == 1
/** A mask that filters the sign bit of the externally known, shorter signed integer type. */
# define RAT_SIGN_BIT_RAT_SIGNED_INT 0x80000000l
#else
/** A mask that filters the sign bit of the externally known, shorter signed integer type. */
# define RAT_SIGN_BIT_RAT_SIGNED_INT 0x8000l
#endif

/** The rational number null. */
#define RAT_NULL ((rat_num_t){.n = 0, .d = 1})

/** The rational number 1. */
#define RAT_ONE ((rat_num_t){.n = 1, .d = 1})

/** The rational number minus 1. */
#define RAT_MINUS_ONE ((rat_num_t){.n = -1, .d = 1})

/* Range of the signed integer and longer signed integer type used in the implementation of
   rational numbers. */
#if RAT_USE_64BIT_ARITHMETIC == 1
# define RAT_SIGNED_INT_MAX      (LONG_MAX)
# define RAT_SIGNED_INT_MIN      (LONG_MIN)
# define RAT_SIGNED_LONG_INT_MAX (LLONG_MAX)
# define RAT_SIGNED_LONG_INT_MIN (LLONG_MIN)
#else /* Use 16/32 Bit representation of rational numbers */
# define RAT_SIGNED_INT_MAX      (SHRT_MAX)
# define RAT_SIGNED_INT_MIN      (SHRT_MIN)
# define RAT_SIGNED_LONG_INT_MAX (LONG_MAX)
# define RAT_SIGNED_LONG_INT_MIN (LONG_MIN)
#endif /* Short or long representation. */


/*
 * Global type definitions
 */

#if RAT_USE_64BIT_ARITHMETIC == 1
/** The type of the numerators and denominators of all rational numbers. Basically any
    signed int type could be used, but we implement a simple overflow recognition by using
    signed long int and thus having signed long long int as kind of internal reserve to
    inspect results for being too large. */
typedef signed long int rat_signed_int;

/** The type of integers used for rational numbers must not be the longest available type.
    A longer type is defined here to be used for internal overflow recognition. */
typedef signed long long int rat_signed_long_int;
#else
typedef signed short int rat_signed_int;
typedef signed long int rat_signed_long_int;
#endif


/** A rational number. */
typedef struct rat_num_t
{
    /** The numerator. */
    rat_signed_int n;

    /** The denominator. */
    rat_signed_int d;
    
} rat_num_t;



/*
 * Global data declarations
 */

/** A global flag to indicate overflows.
      @remark Despite of its global declaration is this a module internal variable. Never
    touch this variable! */
extern boolean rat_overflowFlag;


/*
 * Global inline interface
 */

/**
 * Return the sign of the rational number.
 *   @return
 * Get either 1 or -1. The sign of null is undefined, both values can be returned.
 *   @param a
 * The number to test.
 */

static inline rat_signed_int rat_sign(rat_num_t a)
{
    if((a.n & RAT_SIGN_BIT_RAT_SIGNED_INT) == (a.d & RAT_SIGN_BIT_RAT_SIGNED_INT))
        return (rat_signed_int)1;
    else
        return (rat_signed_int)-1;
    
} /* End of rat_sign */




/**
 * Compare two rational numbers on equality.
 *   @return
 * Get \a true if the numbers are identical, \a false otherwise.
 *   @param a
 * The first number to compare.
 *   @param b
 * The second number to compare.
 */

static inline boolean rat_isEqual(rat_num_t a, rat_num_t b)
{
    return (rat_signed_long_int)a.n*b.d == (rat_signed_long_int)a.d*b.n;
    
} /* End of rat_sign */




/**
 * Invert the sign of a rational number.
 *   @return
 * Get - \a a.
 *   @param a
 * The number to invert.
 */

static inline rat_num_t rat_neg(rat_num_t a)
{
    /* We can either invert the sign of the numerator of of the denominator. The only
       possible overflow of sign inversion is for the negative maximum integer value. */
    if(a.n != RAT_SIGNED_INT_MIN)
        return (rat_num_t){.n = -a.n, .d = a.d};
    else if(a.d != RAT_SIGNED_INT_MIN)
        return (rat_num_t){.n = a.n, .d = -a.d};
    else
    {
        /* Both integers can't be inverted, which means that they are identical. The
           rational number is one and the correct sign inverted number is minus one. */
        return (rat_num_t){.n = -1, .d = 1};
    }
} /* End of rat_neg */




/**
 * Return the reciprocal value of a rational number.
 *   @return
 * Get 1 / \a a. \a a must not be null.
 *   @param a
 * The number to invert.
 */

static inline rat_num_t rat_reciprocal(rat_num_t a)
{
    assert(a.n != 0);
    return (rat_num_t){.n = a.d, .d = a.n};

} /* End of rat_reciprocal */




/**
 * Return the quotient of two rational numbers.
 *   @return
 * Get \a n / \a d. \a d must not be null.
 *   @param n
 * First operand, the numerator of the quotient.
 *   @param d
 * Second operand, the denominator of the quotient.
 */

static inline rat_num_t rat_div(rat_num_t n, rat_num_t d)
{
    assert(d.n != 0);
    rat_num_t rat_mul(rat_num_t, rat_num_t);
    return rat_mul(n, (rat_num_t){.n = d.d, .d = d.n});

} /* End of rat_div */




/**
 * Euclid's algorithm to find the greatest common divisor of two integer numbers.
 *   @return
 * Get the gcd of \a a and \a b. The sign of gcd is the sign of the operands if they have
 * identical sign or undefined otherwise.
 *   @param a
 * The first numeric operand, a signed integer. If \a a is null then the function returns
 * \a b.
 *   @param b
 * The second numeric operand, a signed integer. If \a b is null then the function returns
 * \a a.
 */

static inline rat_signed_int rat_gcd(rat_signed_int a, rat_signed_int b)
{
    /* See http://de.wikipedia.org/wiki/Euklidischer_Algorithmus and particularly
       http://de.wikipedia.org/wiki/Euklidischer_Algorithmus#Beschreibung_durch_Pseudocode
       (visited on Mar 6, 2014) for details on Euclid's algorithm. */
    while(b != 0)
    {
        /* In C99, h always has the sign of a. */
        rat_signed_int h = a % b;
        a = b;
        b = h; 
    }
    return a;
    
} /* End of rat_gcd */



/**
 * Reset the error flag.
 *   @remark
 * The error flag is global to the module rat, which makes all its functions non re-entrant
 * with respect to proper error recognition. Consider to reset the flag prior to an
 * operation, which you want to have accurate error information for.
 */

static inline void rat_clearError()
{
    rat_overflowFlag = false;
    
} /* End of rat_clearError */




/**
 * Query the error flag.
 *   @return
 * Get the current, Boolean error status.
 *   @remark
 * The error flag is global to the module rat, which makes all its functions non re-entrant
 * with respect to proper error recognition. Serialize your arithmetic operations and use \a
 * rat_clearError to reset the error flag prior to an atomic sequence of operation in order
 * to get accurate error information.
 */

static inline boolean rat_getError()
{
    return rat_overflowFlag;
    
} /* End of rat_getError */




/*
 * Global prototypes
 */

/** Initialize the module prior to the first use of any of its operations or global data
    objects. */
void rat_initModule(log_hLogger_t hGlobalLogger);

/** Do all cleanup after use of the module, which is required to avoid memory leaks,
    orphaned handles, etc. */
void rat_shutdownModule(void);

/** Compute the lowest common multiple of two integer numbers. */
rat_signed_int rat_lcm(rat_signed_int a, rat_signed_int b);

/** Compute the product of two rational numbers. */
rat_num_t rat_mul(rat_num_t a, rat_num_t b);

/** Compute the sum of two rational numbers. */
rat_num_t rat_add(rat_num_t a, rat_num_t b);


#endif  /* RAT_RATIONALNUMBER_INCLUDED */
