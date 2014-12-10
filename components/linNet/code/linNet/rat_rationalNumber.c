/**
 * @file rat_rationalNumber.c
 * Simple numeric class implementing rational numbers.
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
/* Module interface
 *   rat_initModule
 *   rat_shutdownModule
 *   rat_sign (in rat_rationalNumber.h)
 *   rat_gcd (in rat_rationalNumber.h)
 *   rat_clearError (in rat_rationalNumber.h)
 *   rat_getError (in rat_rationalNumber.h)
 *   rat_lcm
 *   rat_mul
 *   rat_add
 * Local functions
 *   reportOverflow
 *   gcdLong
 *   approximate
 *   truncate
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "log_logger.h"
#include "rat_rationalNumber.h"


/*
 * Defines
 */
 
#if RAT_USE_64BIT_ARITHMETIC == 1
/** A mask of bits in the internally used longer integer type, which must not be set in
    proper result values. If a number bit in the mask is found to be set in the preliminary
    result then we decide on overflow. ("Number bit" means: a set bit for positive numbers
    and a null bit for negative numbers.) */
# define OVF_MASK    ((signed long long)0xffffffff80000000)

/** A mask that filters the sign bit of the longer signed integer type. */
# define SIGN_BIT_RAT_SIGNED_LONG ((signed long long)0x8000000000000000)

#else /* Use 16/32 Bit representation of rational numbers */

# define OVF_MASK                    ((signed long)0xffff8000)
# define SIGN_BIT_RAT_SIGNED_LONG    ((signed long)0x80000000)

#endif /* Short or long representation. */


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/** A global logger object is referenced from anywhere for writing progress messages. */
static log_hLogger_t _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

/** A global flag to indicate overflows.
      @remark Despite of its global declaration is this a module internal variable. Never
    touch this variable from outside this module! */
boolean rat_overflowFlag = false;


/*
 * Function implementation
 */

/**
 * Report a recognized overflow. Currently, this is done by setting a global error flag and
 * writing a message into the global application log if it wasn't set before.\n
 *   @param n
 * The numerator of the "bad" number causing the error message is passed in for reporting
 * purpose.
 *   @param d
 * The denominator of the "bad" number causing the error message is passed in for reporting
 * purpose.
 */ 

static void reportOverflow(rat_signed_long_int n, rat_signed_long_int d)
{
    if(!rat_overflowFlag)
    {
        rat_overflowFlag = true;

        /* Portable code: GCC on MinGW doesn't support the printf formatting character %lld
           but requires %I64d, see
           http://stackoverflow.com/questions/13590735/printf-long-long-int-in-c-with-gcc,
           Feb 2014. */
#if defined(__WIN32) || defined(__WIN64)
# define F64D    "%I64d"
#else
# define F64D    "%lld"
#endif
        LOG_FATAL( _log
                 , "Arithmetic overflow during computation. Number " F64D "/" F64D 
                   " = %.15g can't be represented by objects of class rat_num"
                 , (signed long long)n, (signed long long)d
                 , (double)n/d
                 );
#undef F64D
    }
} /* End of reportOverflow */




/**
 * Euclid's algorithm to find the greatest common divisor of two integer numbers. This
 * second implementation of the same algorithm is used internally to implement safe
 * overflow recognition.
 *   @return
 * Get the gcd of \a a and \a b. The sign of gcd is the sign of the operands if they have
 * identical sign or undefined otherwise.
 *   @param a
 * The first numeric operand, a signed integer. If \a a is null then the function returns
 * \a b.
 *   @param b
 * The second numeric operand, a signed integer. If \a b is null then the function returns
 * \a a.
 *   @see rat_signed_int rat_gcd(rat_signed_int, rat_signed_int)
 */

static inline rat_signed_long_int gcdLong(rat_signed_long_int a, rat_signed_long_int b)
{
    /* See http://de.wikipedia.org/wiki/Euklidischer_Algorithmus and particularly
       http://de.wikipedia.org/wiki/Euklidischer_Algorithmus#Beschreibung_durch_Pseudocode
       (visited on Mar 6, 2014) for details on Euclid's algorithm. */
    while(b != 0)
    {
        /* In C99, h always has the sign of a. */
        rat_signed_long_int h = a % b;
        a = b;
        b = h; 
    }
    return a;
    
} /* End of gcdLong */



/**
 * Approximate a rational number, which is not in the set of representable ones by a close
 * representable one.
 *   @return
 * The appromiated rational number is returned.
 *   @param n
 * The wanted numerator. The size of the passed integer is longer than the type use for the
 * representation of rational numbers.
 *   @param d
 * The wanted denominator. The size of the passed integer is longer than the type use for the
 * representation of rational numbers.
 */ 

static rat_num_t approximate(rat_signed_long_int n, rat_signed_long_int d)
{
#ifdef DEBUG
    const double number = (double)n/d;
//printf("n_in: %ld, d_in: %ld\n", n, d);
#endif
    /* Underflow recognition is easier, if we only have positive numbers. */
    boolean isNegative;
    if(n < 0)
    {
        isNegative = true;
        
        /* We can avoid an overflow in the sign inversion by (absolutely) decrementing the
           number by one - this is still a very good aproximation. */
        if(n == RAT_SIGNED_LONG_INT_MIN)
            n = RAT_SIGNED_LONG_INT_MAX;
        else
            n = -n;
    }
    else
        isNegative = false;

    if(d < 0)
    {
        isNegative = !isNegative;
        
        /* We can avoid an overflow in the sign inversion by (absolutely) decrementing the
           number by one - this is still a very good aproximation. */
        if(d == RAT_SIGNED_LONG_INT_MIN)
            d = RAT_SIGNED_LONG_INT_MAX;
        else
            d = -d;
    }

    /* Basic idea: Numerator and denominator are divided by two and tested again. The
       division could lead to a (formerly not available gcd > 1) so that already single bit
       shift could make the number fit. However, the chance is little, that this happens.
       As a compromise between best result and reduced number of attempts, we decide to
       shift by 4 in each divide-and-test step. */
    do
    {
        n >>= 4;
        d >>= 4;
        
        /* We can have an underflow in either number. If n is affected, we end up with the
           correct approximation 0. An underflow of d needs to be handled; denominator null
           is forbidden. */
        if(d == 0)
        {
            /* The underflow of d happened because the number is out of the range of
               representable numbers. We approximate it by the largest representable number
               we have. */
            if(isNegative)
            {
                n = RAT_SIGNED_INT_MIN;
                isNegative = false;
            }
            else
                n = RAT_SIGNED_INT_MAX;

            d = 1;
            break;
        }
        
        /* If n==0 then the gcd c will become d and the correct result is represented by
           0/1, which is perfect. */
        const rat_signed_long_int c = gcdLong(n, d);
        if(c != 1)
        {
            n /= c;
            d /= c;
        }
        
//printf("n>>4: %ld, d>>4: %ld\n", n, d);
    }
    while((n & OVF_MASK) != 0  ||  (d & OVF_MASK) != 0);
    
    /* No danger of overflow in sign inversion here, as we will only make positive numbers
       negative. */
    if(isNegative)
        n = -n;

#ifdef DEBUG
    LOG_DEBUG( _log
             , "rat_rationalNumber::approximate: %.15g is approximated by %ld/%ld = %.15g"
             , number
             , n, d
             , (double)(rat_signed_int)n/(double)(rat_signed_int)d
             );
#endif

    return (rat_num_t){.n = (rat_signed_int)n, .d = (rat_signed_int)d};
    
} /* End of approximate */




/**
 * Check a number for overflow. The principle is trivial. We first do the operation in an
 * integer size, which is larger than the result type. Then we test the operation result if
 * it would also fit into the shorter result type and truncate it safely. The test done by
 * this function.\n
 *   If an overflow is recognized then it is reported by side effect in the global error
 * variable, which the client can query after his computations. This way of doing is more
 * efficient than permanent return code checks but makes the operations implemented in this
 * module non re-entrant.
 *   @return
 * The safely truncated number \a a: either the same number or the (unavoidable) error has
 * been reported and an approximation of \a a is returned.
 *   @param n
 * The numerator of the rational number to be checked and truncated.
 *   @param d
 * The denominator of the rational number to be checked and truncated.
 */ 

static inline rat_num_t truncate(rat_signed_long_int n, rat_signed_long_int d)
{
    const rat_signed_long_int upperHalfWordN = n & OVF_MASK
                            , upperHalfWordD = d & OVF_MASK;
    if((upperHalfWordN == 0  ||  upperHalfWordN == OVF_MASK)
       && (upperHalfWordD == 0  ||  upperHalfWordD == OVF_MASK)
      )
    {
        return (rat_num_t){.n = (rat_signed_int)n, .d=(rat_signed_int)d};
    }
    else
    {
        reportOverflow(n, d);
        return approximate(n, d);
    }
} /* End of truncate */




/**
 * Initialize the module at application startup.
 *   @param hLogger
 * This module will use the passed logger object for all reporting during application life
 * time. It must be a real object, LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT is not permitted.
 *   @remark
 * Do not forget to call the counterpart at application end.
 *   @remark
 * This module depends on the other module log_logger. It needs to be initialized after
 * this other module.
 *   @remark Using this function is not an option but a must. You need to call it
 * prior to any other call of this module and prior to accessing any of its global data
 * objects.
 *   @see void rat_shutdownModule()
 */

void rat_initModule(log_hLogger_t hLogger)
{
    assert(hLogger != LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT);
    _log = log_cloneByReference(hLogger);

} /* End of rat_initModule */




/**
 * Do all cleanup after use of the module, which is required to avoid memory leaks, orphaned
 * handles, etc.
 */

void rat_shutdownModule()
{
    /* Discard reference to the logger object. */
    log_deleteLogger(_log);
    _log = LOG_HANDLE_TO_EMPTY_LOGGER_OBJECT;

} /* End of rat_shutdownModule */




/**
 * Compute the lowest common multiple of two numbers.\n
 *   The operation can overflow. In this case an error is reported and the closest
 * approximation is returned - which will be useless in nearly all use cases. You will have
 * to check the global error flag.
 *   @return
 * Get the LCM. The sign is positive if both operands have the same size and undefined if
 * they have differing signs. If at least one operand is null then the LCM is defined to be
 * also null.
 *   @param a
 * The first numeric operand, a signed integer.
 *   @param b
 * The second numeric operand, a signed integer.
 */

rat_signed_int rat_lcm(rat_signed_int a, rat_signed_int b)
{
    /* The LCM is the product of two numbers divided by their GCD. As the product alwaxs
       exists in the larger number range and as the division is always possible, the lcm
       can always safely be computed in this range just like that. */
    rat_signed_int gcd = rat_gcd(a, b);
    
    /* The only pit fall: If a and b are null then the gcd becomes null also and the
       division will fail. */
    if(gcd == 0)
    {
        /* a and b are null and the LCM is defined to be null also. */
        assert(a == 0  &&  b == 0);
        return 0;
    }
    
    /* The division is done first, then it can be computed in the shorter word size. */
    assert(a % gcd == 0);
    a /= gcd;
    
    rat_signed_long_int p = (rat_signed_long_int)a * b;
    
    /* Truncate result to shorter, externally known range. This can easily cause an
       overflow. */
    const rat_signed_long_int upperHalfWordP = p & OVF_MASK;
    if(upperHalfWordP == 0  ||  upperHalfWordP == OVF_MASK)
        return (rat_signed_int)p;
    else
    {
        reportOverflow(p, 1);
        return p>0? RAT_SIGNED_INT_MAX: RAT_SIGNED_INT_MIN;
    }
} /* End of rat_lcm */




/**
 * Compute the product of two rational numbers.
 *   @return
 * Get \a a times \a b.
 *   @param a
 * The first operand, a rational number.
 *   @param b
 * The second operand, a rational number.
 */

rat_num_t rat_mul(rat_num_t a, rat_num_t b)
{
    /* The product can be computed safely in the longer integer size. */
    rat_signed_long_int n = (rat_signed_long_int)a.n * b.n
                      , d = (rat_signed_long_int)a.d * b.d;
    
    /* The Euclid algorithm can safely be used in the longer integer size oto cancel the
       ratio. */
    rat_signed_long_int c = gcdLong(n, d);
    return truncate(n/c, d/c);

} /* End of rat_mul */




/**
 * Compute the sum of two rational numbers.
 *   @return
 * Get \a a plus \a b.
 *   @param a
 * The first operand, a rational number.
 *   @param b
 * The second operand, a rational number.
 */

rat_num_t rat_add(rat_num_t a, rat_num_t b)
{
    /* The denominator is a product, which can safely be computed in the longer integer
       size. */
    rat_signed_long_int d = (rat_signed_long_int)a.d * b.d;

    /* The numerator of the sum is the sum of two products. Each of these can be safely
       computed in the longer integer size. */
    rat_signed_long_int n1 = (rat_signed_long_int)a.n * b.d
                      , n2 = (rat_signed_long_int)a.d * b.n
                      , n = n1 + n2;
                      
    /* The sum of the two numerator terms has been safely done if both operands have
       different signs or if the sum didn't undergo a sign inversion. */
    if(((n1^n2) & SIGN_BIT_RAT_SIGNED_LONG) == 0
       &&  ((n^n1) & SIGN_BIT_RAT_SIGNED_LONG) != 0
      )
    {
        /* Error: The computed sum n caused an overflow. n must not be used. */

        /* The sum can surely be computed if we shift both operands one to the right. As
           this means a division by two we have to divide the denominator by two, too. The
           result is a good approximation but an error needs to be reported. */
        n = (n1>>1) + (n2>>1);
        d >>= 1;
        
        /* Acceptable weakness: There's a significant chance that the truncate operation
           below will report the same error once again. */
        reportOverflow(n, d);
    }

    /* The Euclid algorithm can safely be used in the longer integer size to cancel the
       ratio. */
    rat_signed_long_int c = gcdLong(n, d);

    /* Go back to result range and check for overflow. */
    return truncate(n/c, d/c);
    
} /* End of rat_add */




