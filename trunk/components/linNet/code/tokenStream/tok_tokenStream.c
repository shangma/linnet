/**
 * @file tok_tokenStream.c
 * This module implements a token scanner for lexical symbols as used in the language C and
 * many other languages. The input is parsed and returned to the client as a sequence of
 * tokens.\n
 *   The syntax definitions is somewhat open to configuration. Particular single
 * characters or sequences of such can be defined to become named tokens. This way
 * application dependent keywords and operators can be defined. Some syntax elements are
 * hard coded and can't be re-defined, examples are number literals or identifiers.\n
 *   The hard coded built-in syntax elements have priority over the client configured
 * tokens. This means the latter cannot begin with a sequence of characters that resembles
 * a built-in element, followed by some other characters, which would make them basically
 * distinguishable: For example abc++cba is not supported as a user configured symbol. The
 * scanner would first recognize an identifier abc and then fail to recognize the rest.
 * Same for 'q, "xx or 23a, which are conflicting with character constants (beginning with
 * '), string literals (beginning with ") and numerals (beginning with a digit). ++abc
 * would however be a valid custom symbol.\n
 *   Binary literals are an option (%1001) and suffix multipliers are an option (3.45k)
 * but for floating point numerals only.\n
 *   String constants can be enclosed in single or double quotes. However, if single quotes
 * are configured, then no C like character constants are recognized - they would be
 * returned as strings of length one.\n
 *   The end of line character can be handled as (ignored) white space or as recognized and
 * returned token.\n
 *   Comments can be enclosed in a pair of tags or they have a starting tag and end in
 * front of the next end of line character. All three needed tags can be configured.\n
 *   As long as no syntax error is found the scanner always returns a token. If it cannot
 * match an input character to any of the more complex token types it'll return the
 * character itself as token, i.e. the token type is set to the character code. This design
 * decision has been taken because most parsers have to handle a lot of single character
 * tokens like parenthesis and these are implicitly defined.
 *
 * Copyright (C) 1992-2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   tok_createTokenStream
 *   tok_deleteTokenStream
 *   tok_setBoolOption
 *   tok_getErrorMsg
 *   tok_resetError
 *   tok_getLine
 *   tok_fprintfToken
 *   tok_getNextToken
 * Local functions
 *   smalloc
 *   stralloccpy
 *   boolOption
 *   optionEscapeChar
 *   optionStringQuote
 *   isodigit
 *   readCharFromStream
 *   peekChar
 *   nextRawChar
 *   nextChar
 *   currentChar
 *   readStringLiteral
 *   readString
 *   readNumeral
 *   cmpTokenWithToken
 *   cmpTokenWithKey
 *   readIdentifier
 *   readCharacterConstant
 *   readCustomSymbol
 *   readComment
 *   createTokenDescriptorTable
 *   deleteTokenDescriptorTable
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "types.h"
#include "smalloc.h"
#include "snprintf.h"
#include "fio_fifoChar.h"
#include "tok_tokenStream.h"


/*
 * Defines
 */

/** Some compiler environments do not know the binary search routine. Then define the next
    symbol to be 1 and compile a local implementation. */
#define USE_LOCAL_BSEARCH   0

#if USE_LOCAL_BSEARCH
# define BSEARCH new_bsearch
#else
# define BSEARCH bsearch
#endif

/** The end of line indicator. */
#define EOL ((signed int)'\n')


/** Use a constant number as string literal, e.g. for static, compile time status messages. */
#define NUM_LITERAL_TO_STR(num)  INTERNAL_TO_STR(num)
#define INTERNAL_TO_STR(num) #num


/*
 * Local type definitions
 */

/** Some syntax construct of the tokenizer are optional, i.e. they can be switched on or
    off in order to customize the behavior. */
typedef struct syntaxOptions_t
{
    /** Decide, whether the end of line is ordinary white space only or a recognized token. */
    boolean eolIsWhiteSpaceOnly;

    /** Support binary number literals like %01101. */
    boolean binLiteral;

    /** Support 1-char exponent for floating point numbers, like 23.4u for 23.4e-6. */
    boolean suffixMultipliers;

    /** Select the escape character for character constants in strings. */
    signed int escapeChar;

    /** Strings can be enclosed in either double or single quotes. */
    signed int stringQuote;

} syntaxOptions_t;



/** The token stream. */
typedef struct tok_tokenStream_t
{
    /** The name of the scanned input file is saved for logging purpose.\n
          The string is malloc allocated. */
    const char *fileName;

    /** The current line number. */
    unsigned int line;

    /** The current error status. The error is set when the parser fails to relate an input
        character to any defined token and is reset when querying the error status via
        the class interface. */
    boolean error;

    /** Writing error messages in a log is an option only. The scanner also holds the error
        messages in a string variable. This makes the scanner independent of an existing
        application log.\n
          The string is malloc allocated. */
    const char *errorMsg;

    /** If the scanner read a normal file stream then the file object is stored in this
        field. Otherwise it is an handle to the stream of the application defined character
        input function. */
    tok_hStream_t hStream;

    /** The application defined character input function if in use, or NULL otherwise. */
    int (*getc)(tok_hCharInputStream_t);

    /** The customer provided extension of the internal token table. The application my
        define particular character sequences and associate them with an integer number
        meaning the kind of token. */
    tok_tokenDescriptorTable_t tokenDescriptorTable;

    /** The custimizing options of the tokenizer. */
    syntaxOptions_t options;

    /** Internal variable of character read state machine: Flag that indicates that the
        next read character belongs to the next line. */
    boolean incLine;

    /** Internal variable of character read state machine: Flag that indicates that a
        character has already been read from the actual state (for a look ahead). */
    boolean peeked;

    /** Internal variable of character read state machine: If the look ahead had been used,
        then this Variable holds the next character, not the stream. */
    signed int peekChar;

    /** Internal variable of character read state machine: The last recently read character
        if the scanner would like to see it again. */
    signed int currentChar;

    /** Internal variable: Temporarily used character FIFO. The object is created once and
        reused during the life-time of the token stream object. It is the basis of managing
        strings of arbitrary length. */
    fio_hFifoChar_t hFifoChar;

} tok_tokenStream_t;


/*
 * Local prototypes
 */


/*
 * Data definitions
 */


/*
 * Function implementation
 */

#if USE_LOCAL_BSEARCH
/**
 * Depending on the compile environment the binary sreach function bsearch could be
 * unavailable. If so, use this substitute by setting the macro #USE_LOCAL_BSEARCH.\n
 *   The function implements the bianry search on a sorted array.
 *   @return
 * Get the pointer to the array element holding the wanted contents. Or NULL if no such
 * element exists.
 *   @param key
 * The contents it is looked for in the array elements.
 *   @param array
 * The searched array.
 *   @param sizeOfArray
 * The number of elements in \a array.
 *   @param sizeOfElement
 * The size in Byte of each element in the array, e.g. sizeof(array[0]).
 *   @param compare
 * The array contents are opaque to the algorithm, but it is based on the order of elements
 * with respect to an also unknown ordering criterion. Therfore the comparison itself is
 * provided by the client of this function. The function to pass is defined as follows:\n
 *   It takes the key value, we look for and an array element, both passed by reference.\n
 *   It compares the array element with the key and returns -1, 0, 1 for the cases key <
 * array element, key == array element, key > array element - whatever <, ==, > means for
 * the given key and element type.
 */

static const void *new_bsearch( const void * const pKey
                              , const void * const array
                              , size_t sizeOfArray
                              , size_t sizeOfElement
                              , signed int (* const compare)( const void * const pKey
                                                            , const void * const pElement
                                                            )
                              )
{
    signed int idxFirst = 0
             , idxLast = sizeOfArray-1;

    while(idxFirst <= idxLast)
    {
        unsigned int idxMiddle = (idxFirst + idxLast) / 2;

        /* Compute pointer to the new middle of the still searched area. */
        const void *pMiddle = (const void*)((const char*)array + idxMiddle*sizeOfElement);

        /* Compare middle element with key value. */
        signed int cmpRes = compare(pKey, pMiddle);
        if(cmpRes > 0)
        {
            /* Middle element is too small: It becomes the new lower boundary of the search
               area (+1: exclusively). */
            idxFirst = idxMiddle + 1;
        }
        else if(cmpRes < 0)
        {
            /* Middle element is too great: It becomes the new upper boundary of the search
               area (-1: exclusively). */
            idxLast = idxMiddle - 1;
        }
        else
        {
            /* The tested element matches: immediate return. */
            return pMiddle;
        }
    } /* End while(Search area is still not empty) */

    /* Not found, result NULL. */
    return NULL;

} /* End of new_bsearch */
#endif



/**
 * Get a boolean option of the tokenizer.
 *   @return
 * True or flase, the current setting of the option.
 *   @param hTStream
 * The token stream object to query.
 *   @param option
 * Pass in, which Boolean option to return.
 */

static inline boolean boolOption(const tok_hTokenStream_t hTStream, tok_syntaxOption_t option)
{
    switch(option)
    {
    case tok_optionEolIsWhiteSpaceOnly: return hTStream->options.eolIsWhiteSpaceOnly;
    case tok_optionBinLiteral: return hTStream->options.binLiteral;
    case tok_optionSuffixMultipliers: return hTStream->options.suffixMultipliers;
    case tok_optionStringsUseSingleQuotes: return hTStream->options.stringQuote == '\'';

    default: assert(false); return false;
    }
} /* End of boolOption */




/**
 * Query option tok_optionEscapeCharIsDollarSign of the tokenizer.
 *   @return
 * Get the selected escape character for character constants.
 *   @param hTStream
 * The token stream object to query.
 */

static inline signed int optionEscapeChar(const tok_hTokenStream_t hTStream)
{
    return hTStream->options.escapeChar;

} /* End of optionEscapeChar */




/**
 * Query option tok_optionStringsUseSingleQuotes of the tokenizer.
 *   @return
 * Get the selected quote character for string literals.
 *   @param hTStream
 * The token stream object to query.
 */

static inline signed int optionStringQuote(const tok_hTokenStream_t hTStream)
{
    return hTStream->options.stringQuote;

} /* End of optionStringQuote */




/**
 * Decide, whether a character is an octal digit (0..7).
 *   @return
 * Get the Boolean answer.
 *   @param c
 * The character to test.
 */

static inline boolean isodigit(signed int c)
{
    return c >= '0' &&  c <= '7';

} /* End of isodigit */






/**
 * Decide, whether a character is white space. This function replaces the function isspace
 * from the standard library because it treats the end of line differently: This is a token
 * of particular interest by default.
 *   @return
 * Get the Boolean answer.
 *   @param hTStream
 * The token stream object, which is needed to access the option whether EOL is white space
 * or not.
 *   @param c
 * The character to test.
 */

static inline boolean iswhitespace(tok_hTokenStream_t hTStream, signed int c)
{
    return c == ' '  ||  c == '\t'
           ||  (boolOption(hTStream, tok_optionEolIsWhiteSpaceOnly) &&  c == '\n')
           ||  c == '\v'  ||  c == '\f'  ||  c == '\r';

} /* End of iswhitespace */




/**
 * Read the next character from the actual stream, be it a stdio stream or an externally
 * implemented custom stream.
 *   @return
 * Get the next character, which might be EOF to indicate the end of the parsing process.
 *   @param hTokenStream
 * The token stream object to operate on.
 */

static signed int readCharFromStream(tok_hTokenStream_t hTokenStream)
{
    signed int c;
    if(hTokenStream->getc == NULL)
    {
        assert(hTokenStream->hStream.hFile != NULL);
        c = fgetc(hTokenStream->hStream.hFile);

        /* The stdio stream returns EOF if we actually have a stream error. An additional
           check is needed in case. */
        if(c == EOF  &&  feof(hTokenStream->hStream.hFile) == 0)
        {
#define ERR_MSG_FSTR "Stream error in C stdio library, error number %d"
            signed int err = ferror(hTokenStream->hStream.hFile);
            const size_t maxStrLen = sizeof(ERR_MSG_FSTR) + sizeof("-2147483648");
            char *errMsg = smalloc(maxStrLen, __FILE__, __LINE__);
            snprintf(errMsg, maxStrLen, ERR_MSG_FSTR, err);
            hTokenStream->error = true;
            free((char*)hTokenStream->errorMsg);
            hTokenStream->errorMsg = errMsg;
#undef ERR_MSG_FSTR
        }
    }
    else
    {
        assert(hTokenStream->hStream.hCustomStream != NULL);
        c = hTokenStream->getc(hTokenStream->hStream.hCustomStream);
    }

    return c;

} /* End of readCharFromStream */




/**
 * Look at the next character in the stream. This is the character, the next call of signed
 * int nextChar(tok_hTokenStream_t hTokenStream) will return.\n
 *   The call of this function does not iterate versus the future stream characters. If it
 * is called repeatedly it'll always return the same character, which is the next one until
 * nextChar is called.
 *   @return
 * Get the preview on the next character in the stream. The character is however not
 * consumed from the stream.
 *   @param hTokenStream
 * The token stream object to operate on.
 */

static signed int peekChar(tok_hTokenStream_t hTokenStream)
{
    /* If we already had peeked, we return the same character again. Only otherwise get a
       character from the stream. */
    if(!hTokenStream->peeked)
    {
        hTokenStream->peekChar = readCharFromStream(hTokenStream);
        hTokenStream->peeked = true;
    }

    return hTokenStream->peekChar;

} /* End of peekChar */




/**
 * Get the next character from the input stream. Normally the other function nextChar will
 * be used by the scanner. This function is a bit more low level: It doesn't consider a
 * line concatenation by the pair backslash+NL, but will return both characters as such in
 * subsequent calls.
 *   @return
 * The next character, which might be EOF to indicate the end of the parsing process.
 *   @param hTokenStream
 * The token stream object to operate on.
 */

static signed int nextRawChar(tok_hTokenStream_t hTokenStream)
{
    /* If the previous call of this method had returned an EOL character then we now
       increment the line number. This way the EOL character itself belongs to the line it
       closes. */
    if(hTokenStream->incLine)
    {
        ++ hTokenStream->line;
        hTokenStream->incLine = false;
    }

    signed int c;
    if(hTokenStream->peeked)
    {
        hTokenStream->peeked = false;
        c = hTokenStream->peekChar;
    }
    else
        c = readCharFromStream(hTokenStream);

    if(c == EOL)
        hTokenStream->incLine = true;

    return c;

} /* End of nextRawChar */




/**
 * Get the next character from the input stream. Line concatenation by backslash+EOL is
 * handled by the function, so it's transparent to the caller (besides an increment of the
 * line number counter). Which means that a comment of type till-end-of-line can also
 * benefit from line concatenation.
 *   @return
 * The next character, which might be EOF to indicate the end of the parsing process.
 *   @param hTokenStream
 * The token stream object to operate on.
 */

static signed int nextChar(tok_hTokenStream_t hTokenStream)
{
    while(true)
    {
        signed int c = nextRawChar(hTokenStream);

        /* Is the read character part of the line concatenation pair backslash+EOL? */
        if(c == '\\' &&  peekChar(hTokenStream) == EOL)
        {
            /* Yes it is, ignore both characters as if not present in the stream. The peeked
               EOL character is consumed from the stream. */
            nextRawChar(hTokenStream);
        }
        else
        {
            hTokenStream->currentChar = c;
            return c;
        }
    }
} /* End of nextChar */





/**
 * Return the last recently read input character once again. The stream is not accessed at
 * all.
 *   @return
 * Get the same character as got in the preceding call of int nextChar(tok_hTokenStream_t)
 * or EOF if the other function had not been called yet.
 *   @param hTokenStream
 * The token stream object to operate on.
 */

static inline signed int currentChar(tok_hTokenStream_t hTokenStream)
{
    return hTokenStream->currentChar;

} /* End of currentChar */




/**
 * Read the value of a character, that is defined by an expression starting with the
 * backslash. The backslash was already consumed from the input stream by the calling
 * function.
 *   @return
 * The value of the character is returned.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 */

static signed int readEscapedChar(tok_hTokenStream_t hTStream)
{
    signed int ch = currentChar(hTStream)
             , chValue = 0;
    unsigned int u;

    /* The character value can be expressed by an up to two digit long hexadecimal number. */
    if(toupper(ch) == 'X')
    {
        ch = nextChar(hTStream);
        for(u=0; u<2 && isxdigit(ch); ++u)
        {
            signed int digitValue;
            digitValue = toupper(ch);
            if(digitValue >= 'A')
                digitValue = 10 + digitValue - 'A';
            else
                digitValue = digitValue - '0';
            assert((digitValue & ~0xf) == 0);

            chValue = (chValue << 4) | digitValue;
            ch = nextChar(hTStream);
        }
        assert(chValue <= 255);

        if(u == 0)
        {
            hTStream->error = true;
            free((char*)hTStream->errorMsg);
            hTStream->errorMsg = stralloccpy("Invalid hexadecimal character constant."
                                             " Expect one or two hexadecimal digits"
                                            );
        }
    }

    /* The character value can be expressed by an up to three digit long octal number. */
    else if(isodigit(ch))
    {
        for(u=0; u<3 && isodigit(ch); ++u)
        {
            chValue = (chValue << 3) | (ch - '0');
            ch = nextChar(hTStream);
        }
        if(chValue > 255)
        {
            hTStream->error = true;
            free((char*)hTStream->errorMsg);
            hTStream->errorMsg = stralloccpy("Octal character constant is out of range");
        }
    }

    /* The current character, the character after the escape character may be a character
       of specific, predefined meaning. */
    else
    {
        switch(toupper(ch))
        {
        case 'A': chValue = '\a'; break;
        case 'B': chValue = '\b'; break;
        case 'F': chValue = '\f'; break;
        case 'N': chValue = '\n'; break;
        case 'R': chValue = '\r'; break;
        case 'T': chValue = '\t'; break;
        case 'V': chValue = '\v'; break;

        /* The character doesn't belong to the set of special characters. Any character
           behind the escape character has the meaning of itself. In particualr, this holds
           for the escape character itself. */
        default: chValue = ch;
        }
        nextChar(hTStream);
    }

    return chValue;

} /* End of readEscapedChar */




/**
 * Read the contents of a string literal. This is everything in between a pair of (not
 * escaped) quotes. The closing quote is not consumed from the input stream. Escape
 * characters inside the string contents are resolved and no longer visible to the caller.
 *   @return
 * The string contents are returned as malloc allocated string.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @see static char *readString(tok_hTokenStream_t)
 */

static char *readStringLiteral(tok_hTokenStream_t hTStream)
{
    fio_hFifoChar_t fifo = hTStream->hFifoChar;

    assert(fio_getNoElements(fifo) == 0);
    const unsigned int startLine = hTStream->line;
    const signed int escapeChar = optionEscapeChar(hTStream)
                   , quoteChar = optionStringQuote(hTStream);
    signed int ch = currentChar(hTStream);
    while(!hTStream->error &&  ch != quoteChar  &&  ch != EOF)
    {
        /* Check string content character for the escape character. If seen the actually
           meant character code is encoded by several (printable) characters. Branch into
           the routine which handled character constants. */
        char stringCh;
        if(ch == escapeChar)
        {
            nextChar(hTStream);
            stringCh = readEscapedChar(hTStream);
            ch = currentChar(hTStream);
        }
        else
        {
            stringCh = (char)ch;
            ch = nextChar(hTStream);
        }

        fio_writeChar(fifo, stringCh);
    }

    if(ch == EOF)
    {
#define ERR_MSG "End of file in string literal beginning on line %u"
        hTStream->error = true;
        free((char*)hTStream->errorMsg);
        char errMsg[sizeof(ERR_MSG)+sizeof("4294967296")];
        snprintf(errMsg, sizeof(errMsg), ERR_MSG, startLine);
        hTStream->errorMsg = stralloccpy(errMsg);
#undef ERR_MSG
    }

    char * const string = smalloc(sizeof(char)*(fio_getNoElements(fifo)+1), __FILE__, __LINE__)
       , *pCh = string;
    while(fio_getNoElements(fifo) > 0)
        * pCh++ = fio_readChar(fifo);
    *pCh = '\000';

    return string;

} /* End of readStringLiteral */




/**
 * Read a complete string constant. As in C, this is the concatenation of (double) quote
 * enclosed string literals, if they are separated by white space only.\n
 *   Attention: It is an option to see the EOL character not as white space but as a token.
 * In this case string literal concatenation becomes rather useless as it is used in
 * practice to break a lengthy string into different lines of source code.
 *   @return
 * The string complete contents are returned as malloc allocated string.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * The read string token is placed into * \a pToken.
 *   @see static char *readStringLiteral(tok_hTokenStream_t)
 */

static void readString(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    assert(currentChar(hTStream) == optionStringQuote(hTStream));

    pToken->type = tok_tokenTypeString;
    pToken->value.string = NULL;

    /* A loop iterates versus all parts of the string; a string may be a
       concatenation of double-quote enclosed fragments, as in C. */
    char *str;
    signed int ch;
    do
    {
        /* Consume the leading double quote. */
        ch = nextChar(hTStream);

        /* Read until EOF or the closing, i.e. not escaped, double quote. */
        str = readStringLiteral(hTStream);

        /* readStringLiteral consumed the characters until it saw the closing
           double quote, therfore we don't need to check for this character here.
           Just consume it. */
        ch = nextChar(hTStream);

        /* Replace the current string contents with the concatenation of so far and
           new. */
        size_t lenSoFar;
        if(pToken->value.string == NULL)
            lenSoFar = 0;
        else
            lenSoFar = strlen(pToken->value.string);
        char *newStr = smalloc(lenSoFar + strlen(str) + 1, __FILE__, __LINE__);
        if(lenSoFar > 0)
            strcpy(newStr, pToken->value.string);
        strcpy(newStr + lenSoFar, str);
        if(pToken->value.string != NULL)
            free((char*)pToken->value.string);
        free(str);
        pToken->value.string = newStr;

        /* Skip white space up to a possible next string fragment.
             It depends on the options, whether the end of a line is also skipped -
           if not the melting of string literals is rather useless. */
        while(iswhitespace(hTStream, ch))
            ch = nextChar(hTStream);
    }
    while(!hTStream->error && ch == optionStringQuote(hTStream));

} /* End of readString */




/**
 * Read the characters from the input stream that belong to and form a numeral. Binary,
 * octal, decimal and hexadecimal literals are supported and floating point numbers.\n
 *   Only positive numbers are considered. A possible sign would be a different, preceding
 * token, to be handled by the client parser.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * The read number token is placed into * \a pToken.
 */

static void readNumeral(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    /* The length of number literals is limited to an absolute number of digits.
       Exceeding this limit generally causes an error. */
#define MAX_LEN_NUMERAL 100
#define INC_IDX                                                                             \
        {   if(idx < MAX_LEN_NUMERAL)                                                       \
                ++ idx;                                                                     \
            else                                                                            \
            {                                                                               \
                hTStream->error = true;                                                     \
                free((char*)hTStream->errorMsg);                                            \
                hTStream->errorMsg = stralloccpy("Numeral is too long. Only up to "         \
                                                 NUM_LITERAL_TO_STR(MAX_LEN_NUMERAL)        \
                                                 " characters are allowed"                  \
                                                );                                          \
            }                                                                               \
        } /* End of macro INC_IDX */

    signed int ch = currentChar(hTStream);

    /* +1: The additional field is used to distinguish normal index increment from
       the case "numeral too long". */
    char numLiteral[MAX_LEN_NUMERAL+1];
    signed int idx = 0;

    enum { typeBinary
         , typeOctal
         , typeDecimal
         , typeFpn
         , typeHex
         } typeOfLiteral;
    const char *digitAry;
    if(boolOption(hTStream, tok_optionBinLiteral)  &&  ch == '%')
    {
        /* Optional binary literal: Consume heading tag %. */
        typeOfLiteral = typeBinary;
        digitAry = "01";
        ch = nextChar(hTStream);
    }
    else if(ch == '0')
    {
        if(peekChar(hTStream) == 'x')
        {
            /* Hexadecimal literal. Consume and store tag 0x. */
            typeOfLiteral = typeHex;
            digitAry = "0123456789ABCDEFabcdef";

            unsigned int u;
            for(u=0; u<2; ++u)
            {
                numLiteral[idx] = (char)ch;
                INC_IDX
                ch = nextChar(hTStream);
            }
        }
        else
        {
            /* Octal literal. Nothing to consume, just set the flag. */
            typeOfLiteral = typeOctal;
            digitAry = "01234567";
        }
    }
    else
    {
        typeOfLiteral = typeDecimal;
        digitAry= "0123456789";
    }

    /* Copy the digits before a possible decimal point. The loop is not entered for
       numbers like .234. */
    while(strchr(digitAry, ch) != NULL)
    {
        numLiteral[idx] = (char)ch;
        INC_IDX
        ch = nextChar(hTStream);
    }

    /* An octal and a decimal literal can become a floating point literal if a
       decimal point or an exponent shows up. (0777. is the valid fpn value 777, not
       an octal followed by next token dot.) */
    if((typeOfLiteral == typeOctal  ||  typeOfLiteral == typeDecimal)
       &&  (strchr(".eE", ch) != NULL
            || (boolOption(hTStream, tok_optionSuffixMultipliers)
                && strchr("yzafpnumcdDhkMGTPXZY", ch) != NULL
               )
           )
      )
    {
        typeOfLiteral = typeFpn;
        pToken->type = tok_tokenTypeFpn;
    }
    else
        pToken->type = tok_tokenTypeInteger;

    /* For floating point numbers read either the fraction or the exponent. */
    signed int fpnPowerOfTen = 0;
    if(!hTStream->error  &&  typeOfLiteral == typeFpn)
    {
        /* Consider a possible decimal dot in case of floating point numbers. */
        if(ch == '.')
        {
            numLiteral[idx] = '.';
            INC_IDX
            ch = nextChar(hTStream);
        }

        /* Read the digits of a fractional part of a floating point number. The loop
           is not entered for integer numbers (all digits had already been consumed by
           the previous loop) and not for floating point numbers like 34. or 34.e2. */
        while(isdigit(ch))
        {
            numLiteral[idx] = (char)ch;
            INC_IDX
            ch = nextChar(hTStream);
        }

        /* An exponent may follow up. In general it may be of form e34 but an option is
           to permit also abbreviated named powers of ten, like k for 10^3. */
        if(ch == 'e'  ||  ch == 'E')
        {
            /* Traditional exponent. */
            numLiteral[idx] = 'E';
            INC_IDX
            ch = nextChar(hTStream);
            if(ch == '-'  ||  ch == '+')
            {
               numLiteral[idx] = (char)ch;
               INC_IDX
               ch = nextChar(hTStream);
            }
            if(!isdigit(ch) && !hTStream->error)
            {
                hTStream->error = true;
                free((char*)hTStream->errorMsg);
                hTStream->errorMsg = stralloccpy("Error in exponent of floating point"
                                                 " numeral. Expect a digit"
                                                );
            }
            else
            {
                do
                {
                    numLiteral[idx] = (char)ch;
                    INC_IDX
                    ch = nextChar(hTStream);
                }
                while(isdigit(ch));
            }

            /* The exponent is entirely in the number literal, no additional factor in
               this case. */
            fpnPowerOfTen = 0;
        }
        else if(boolOption(hTStream, tok_optionSuffixMultipliers)
                && strchr("yzafpnumcdDhkMGTPXZY", ch) != NULL
               )
        {
            /* A single character power-of-ten tag. The table is taken from
               http://searchstorage.techtarget.com/definition/Kilo-mega-giga-tera-peta-and-all-that
               visted on Feb 9, 2014. */
            switch(ch)
            {
            case 'y': fpnPowerOfTen = -24; break; // yocto
            case 'z': fpnPowerOfTen = -21; break; // zepto
            case 'a': fpnPowerOfTen = -18; break; // atto
            case 'f': fpnPowerOfTen = -15; break; // femto
            case 'p': fpnPowerOfTen = -12; break; // pico
            case 'n': fpnPowerOfTen = -9;  break; // nano
            case 'u': fpnPowerOfTen = -6;  break; // micro, u instead of my
            case 'm': fpnPowerOfTen = -3;  break; // milli
            case 'c': fpnPowerOfTen = -2;  break; // centi
            case 'd': fpnPowerOfTen = -1;  break; // deci
            case 'D': fpnPowerOfTen = 1;   break; // deka
            case 'h': fpnPowerOfTen = 2;   break; // hecto
            case 'k': fpnPowerOfTen = 3;   break; // kilo
            case 'M': fpnPowerOfTen = 6;   break; // mega
            case 'G': fpnPowerOfTen = 9;   break; // giga
            case 'T': fpnPowerOfTen = 12;  break; // tera
            case 'P': fpnPowerOfTen = 15;  break; // peta

            /* exa: The reference table tells to use E as suffix. This can't be supported
               since it leads to an ambiguity of expressions like 1E+2: Is this 10^18+2 or
               100? We use the X instead of the E. In practice it won't be any problem. */
            case 'X': fpnPowerOfTen = 18;  break; // exa

            case 'Z': fpnPowerOfTen = 21;  break; // zetta
            case 'Y': fpnPowerOfTen = 24;  break; // yotta
            default: assert(false);
            }
            ch = nextChar(hTStream);

        } /* End if/else if(Power of ten given for floating point number?) */

    } /* End of if(FPN requires scanning of fractional part and exponent?) */

    /* String termination. */
    numLiteral[idx] = '\0';

    /* Convert separated string literal into number. */
    if(!hTStream->error)
    {
        if(typeOfLiteral == typeFpn)
        {
            pToken->value.fpn = atof(numLiteral);
            pToken->value.fpn *= pow(10.0, (double)fpnPowerOfTen);
        }
        else
        {
            assert(pToken->type == tok_tokenTypeInteger);

            /* According to http://www.cplusplus.com/reference/string/stoull/?kw=stoull,
               visitied Feb 9, 2014, stoull would permit to support 64 Bit integers.
               However, this function is not available in GCC of mingw32 nor mingw64 at
               that time. Probably, it's avalaible in C++ only. */

            /* The conversion routine considers the leading 0 for octal and the 0x
               for hexadecimal (and assumes decimal otherwise) if base is set to 0;
               the binary format requires conversion with explicitly set base. */
            signed int base = typeOfLiteral == typeBinary? 2: 0;
            pToken->value.integer = strtoul(numLiteral, NULL, base);
        }
    }
#undef INC_IDX
#undef MAX_LEN_NUMERAL
} /* End of readNumeral */




/**
 * Comparsion of two tokens in the token description table: Needed to sort the table
 * initially. See qsort for more.
 *   @return
 * <0: The element pointed by \a p1 goes before the element pointed by \a p2.\n
 *  0: The element pointed by \a p1 is equivalent to the element pointed by \a p2.\n
 * >0: The element pointed by \a p1 goes after the element pointed by \a p2.\n
 *   @param p1
 * Points to the first token description.
 *   @param p2
 * Points to the second token description.
 *   @remark
 * This function is solely used by bsearch to look for a found identifier in the given
 * table of known keywords.
 */

static signed int cmpTokenWithToken( const void /* (tok_tokenDescriptor_t) */ *p1
                                   , const void /* (tok_tokenDescriptor_t) */ *p2
                                   )
{
   return strcmp( ((const tok_tokenDescriptor_t*)p1)->symbol
                , ((const tok_tokenDescriptor_t*)p2)->symbol
                );

} /* End of cmpTokenWithToken */




/**
 * Comparsion of a token in the token description table with a key. The key is the symbol
 * (a string) represented by the token. The comparison is needed to do a binary search for
 * a given token in the table of client defined symbols. See bsearch for more.
 *   @return
 * <0: The element pointed by \a pKey goes before the element pointed by \a pElem.\n
 *  0: The element pointed by \a pKey is equivalent to the element pointed by \a pElem.\n
 * >0: The element pointed by \a pKey goes after the element pointed by \a pElem.\n
 *   @param pKey
 * Points to the key to match with \a pElem. * \a pElem will probably have a field, which
 * is matched with the key value.
 *   @param pElem
 * The element to be compared with the key.
 *   @remark
 * This function is solely used by bsearch to look for a found identifier in the given
 * table of known keywords.
 */

static signed int cmpTokenWithKey( const void /* (char) */ *pKey
                                 , const void /* (tok_tokenDescriptor_t) */ *pElem
                                 )
{
   return strcmp((const char*)pKey, ((const tok_tokenDescriptor_t*)pElem)->symbol);

} /* End of cmpTokenWithKey */




/**
 * Read the characters from the input stream that belong to and form an identifier.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * The read symbol is placed into * \a pToken.
 */

static void readIdentifier(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    fio_hFifoChar_t fifo = hTStream->hFifoChar;
    assert(fio_getNoElements(fifo) == 0);

    signed int ch = currentChar(hTStream);
    do
    {
        fio_writeChar(fifo, ch);
        ch = nextChar(hTStream);
    }
    while(isalnum(ch) || ch=='_');

    char ident[fio_getNoElements(fifo)+1]
       , *pCh = ident;
    while(fio_getNoElements(fifo)>0)
        * pCh++ = fio_readChar(fifo);
    *pCh = '\000';

    /* Keywords are syntactically identical to identifiers but they have of course the
       higher priority in the token recognition. Search the static table of keywords for
       the just isolated identifier. */
    const void *resSearch = BSEARCH( ident
                                   , hTStream->tokenDescriptorTable.tokenDescriptorAry
                                   , hTStream->tokenDescriptorTable.noTokenDescriptions
                                   , sizeof(tok_tokenDescriptor_t)
                                   , cmpTokenWithKey
                                   );

    /* NULL: Search didn't find the isolated identifier, no keyword. */
    if(resSearch == NULL)
    {
        /* We saw a true identifier, retuen a malloc allocated copy of the string to the
           client. */
        pToken->type = tok_tokenTypeIdentifier;
        pToken->value.identifier = stralloccpy(ident);
    }
    else
    {
        /* We saw a (client defined ) keyword; its representation as an integer is taken
           from the client provided definition table. */
        pToken->type = ((tok_tokenDescriptor_t*)resSearch)->type;
    }

} /* End of readIdentifier */




/**
 * Read the characters from the input stream that belong to and form a character constant
 * as defined in C.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * The read symbol is placed into * \a pToken.
 */

static void readCharacterConstant(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    assert(currentChar(hTStream) == '\'');
    signed int ch = nextChar(hTStream);

    /* Consider the usage of the escape character. */
    if(ch == optionEscapeChar(hTStream))
    {
        nextChar(hTStream);
        pToken->value.character = (char)readEscapedChar(hTStream);
        ch = currentChar(hTStream);
    }
    else
    {
        pToken->value.character = (char)ch;
        ch = nextChar(hTStream);
    }

    if(ch == '\'')
        ch = nextChar(hTStream);
    else
    {
        /* We could already see an error cause by previous function calls. If so don't
           overwrite the message by a now probably failing closing quote message. */
        if(!hTStream->error)
        {
            hTStream->error = true;
            free((char*)hTStream->errorMsg);
            hTStream->errorMsg = stralloccpy("Missing the closing quote (') in a"
                                             " character constant"
                                            );
        }
    }

    pToken->type = tok_tokenTypeCharacter;
    
} /* End of readCharacterConstant */




/**
 * The current input character can't immediately be related to a lexical standard atom so
 * try to match this and the subsequent input characters to a custom symbol.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * The read symbol is placed into * \a pToken.
 */

static void readCustomSymbol(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    signed int ch = currentChar(hTStream);
    
    /* The algorithm is gready: After each character read from the input stream, we search
       the table of custom symbols for a match of the so far read character sequence. As
       soon as such a match is found the acording token is recognized. As a consequence,
       the tokenizer fails if a token is identical to the beginning of a character
       sequence, which is got by concatenating any combination of other tokens (and if the
       syntax definition of the input permits to contain tokens without separating white
       space.) Examples of failures:
         Tokens are ++, ++- and --. The input ++--- can only be tokenized by ++-
       followed by -- but the algorithm implemented here would return ++ followed by
       --. The last - would not be consumed.
         Actually, the last - would be consumed as the final decision of the tokenizer is
       to return any single character as a token, the type of which is the character code.
       (However, this is still an error in the given scenario.)
         Consequently, this function must never return without deciding on a kind of token.
       This is double-checked in DEBUG compilation. */
#ifdef DEBUG
    pToken->type = tok_tokenTypeUnidentified;
#endif

    /* from, to: The range in the table of token descriptors, which still contains match
       candidates; this is the complete table at the beginning (-1: including).
         Remark: Most typical, the table contains a number of symbols, which have
       the form of an identifier. These don't play a role as they would have been
       recognizer already earlier in the check for identifiers. */
    signed int from = 0
             , to   = hTStream->tokenDescriptorTable.noTokenDescriptions - 1;

    /* The index of the character of the symbol, which is compared against the
       table in the current loop cycle. */
    unsigned int idxChar = 0;

    const tok_tokenDescriptor_t * const tokenDescriptorAry =
                                        hTStream->tokenDescriptorTable.tokenDescriptorAry;

    /* Loop over the read characters, which are successively compared with the n-th
       character of the defined, known symbols. */
    while(true)
    {
        /* The beginning of the range of the preceding loop is still required. */
        signed int previousFrom = from;

        /* The search range is norrowed so that it only contains those symbols,
           which in all characters compared so far. This can lead to negative range
           boundaries. */
        while(from <= to  &&  tokenDescriptorAry[from].symbol[idxChar] != ch)
            ++ from;

        while(from <= to  &&  tokenDescriptorAry[to].symbol[idxChar] != ch)
            -- to;

        /* from > to: This character including it can not be a symbol from the table.
           from = to: It could be this symbol but not yet for sure (see next chars).
           from < to: All symbols in the range are still candidates.
             A final decision can already be taken only in the first case. */

        if(from <= to)
        {
            /* No final decision yet, check next input character. */
            ++ idxChar;
            ch = nextChar(hTStream);
        }
        else /* from > to */
        {
            /* This character including it can not be a symbol from the table.
               Check if the sequence of characters excluding the current one are
               identical to a complete symbol.
                 If idxChar == 0, then the current character is not the binning
               character of any defined symbol - by default the tokenizer
               recognizes such charaters as tokens, the value of which is the
               character code. This strongly shortens the table of token
               descriptions as most formal syntaxes use a lot of single-character
               tokens. */
            if(idxChar == 0)
            {
                pToken->type = (tok_tokenType_t)ch;

                /* Done. The read character is a token itself. */
                ch = nextChar(hTStream);
                break;
            }

            /* The symbol at beginning of range of the previous character
               comparison is the only candidate. All other candidates of the
               prvious check (if there are any others) can be excluded as they are
               longer (because of the ASCII sort order of the not equal symbols).
                 Since we already compare all beginning characters of the symbol
               the only open question is whether the symbol candidate in the table
               has additional characters (which would not match the input as we
               already known). */
            if(tokenDescriptorAry[previousFrom].symbol[idxChar] == '\0')
            {
                pToken->type = tokenDescriptorAry[previousFrom].type;
                break;
            }
            else
            {
                /* We saw a character sequence (excluding the current one), which
                   is contained at the beginning of a known symbol, but which
                   doesn't match any symbol completely. In general, this is
                   considered a syntax error (a consequence of the greedy
                   algorithm) but there's an important exception: We made the
                   promise to return single characters as a token, the value of
                   which is the character code. Therefore, if the sequence has
                   length one we don't indicate an error but return the only
                   character of the sequence. */
                if(idxChar == 1)
                {
                    /* The single character to return is still available as we had
                       an according match in the previous loop cylce. */
                    pToken->type = (tok_tokenType_t)tokenDescriptorAry[previousFrom]
                                                    .symbol[0];
                    break;
                }
                else /* syntax error */
                {
                    hTStream->error = true;
                    free((char*)hTStream->errorMsg);
                    hTStream->errorMsg = stralloccpy("Syntax error, scanner can't parse the"
                                                     " input stream because of ambiguous or"
                                                     " undefined symbols. Consider to use"
                                                     " white space to separate the symbols"
                                                    );
                    break;
                }
            } /* End if(Char sequence matched or input doesn't match any symbol) */

        } /* End if(Range already empty? Descision already possible?)  */

    } /* End while(Read as many characters from the input until we can decide) */
    
    assert(hTStream->error ||  pToken->type != tok_tokenTypeUnidentified);
    
} /* End of readCustomSymbol */




/**
 * The beginning of a comment had been recognized. This routine reads the comment contents
 * until and including the comment end tag.
 *   @param hTStream
 * The token stream object, which has the character stream to read from.\n
 *   A possible error indication is returned inside this object.
 *   @param pToken
 * On entry, the token contains the just recognized comment starting token. This function
 * does not change the token as the comment's contents are simply skipped and not returned
 * to the caller.
 */

static void readComment(tok_hTokenStream_t hTStream, const tok_token_t * const pToken)
{
    signed int ch = currentChar(hTStream);
    const unsigned int startLine = hTStream->line;
    
    const boolean lookForEOL = pToken->type == tok_tokenTypeCommentTillEndOfLine;
    boolean continueSearch = true;
    do
    {
        boolean isFirstChar = true;
        const char *pComEndSymbol = lookForEOL
                                    ? "\n"
                                    : hTStream->tokenDescriptorTable.endComment;
        while(!hTStream->error)
        {
            /* Seeing the EOF inside a comment is an error for one type of the
               comments but the end of the search in either case. */
            if(ch == EOF)
            {
                if(!lookForEOL)
                {
#define ERR_MSG "End of file in comment beginning on line %u"
                    hTStream->error = true;
                    free((char*)hTStream->errorMsg);
                    char errMsg[sizeof(ERR_MSG)+sizeof("4294967296")];
                    snprintf(errMsg, sizeof(errMsg), ERR_MSG, startLine);
                    hTStream->errorMsg = stralloccpy(errMsg);
#undef ERR_MSG
                    break;
                }

                /* Leave both loops; the inner by break, the outer by flag. */
                continueSearch = false;
                break;
            }
            else if(ch == (int)*pComEndSymbol)
            {
                /* We still have a match of a sequence of input characters with
                   the comment ending symbol. Advance to the comparison of the
                   next character if the last one has not yet reached. */
                ++ pComEndSymbol;
                if(*pComEndSymbol == '\0')
                {
                    /* End of comment found. */

                    /* Provide next character.
                         By definition, the EOL is both: symbol to end a
                       comment and first character of further scanned input
                       stream. */
                    if(!lookForEOL)
                        nextChar(hTStream);

                    /* Leave both loops; the inner by break, the outer by flag. */
                    continueSearch = false;
                    break;
                }
                else
                {
                    isFirstChar = false;
                    ch = nextChar(hTStream);
                }
            }
            else
            {
                /* No match before we tested all characters of the comment
                   ending symbol. We start the search from scratch; we need to
                   do this with the same input character: it could be the first
                   character of the comment end symbol. If we were somewhere in
                   the middle of the comment end symbol then do not read a new
                   character from the input stream. */
                if(isFirstChar)
                    ch = nextChar(hTStream);

                /* Leave inner loop only, continue the search. */
                break;
            }
        } /* End while(Compare all characters of the comment closing symbol) */
    }
    while(!hTStream->error && continueSearch);

} /* End of readComment */




/**
 * Create the token descriptor table in a newly created token stream object by making a
 * deep copy of the client specified token definition.
 *   @param pTokenStream
 * The handle of the token stream object. The operation is part of the creation of such an
 * object. When calling this function, the object will be incomplete. Only the token
 * definition related parts will be accessed.
 *   @param pCustomTokenDefinition
 * The user provided specification of custom symbols and their token values by reference.
 */

static void createTokenDescriptorTable
                            ( tok_tokenStream_t * const pTokenStream
                            , const tok_tokenDescriptorTable_t * const pCustomTokenDefinition
                            )
{
    if(pCustomTokenDefinition != NULL)
    {
        unsigned int noTokenDescriptions = pCustomTokenDefinition->noTokenDescriptions;
        
        /* The start tags of the possible comments form additional symbols. As comment end
           tag doesn't, it's not considered in general but only if we are inside a
           comment. */
        if(pCustomTokenDefinition->startComment != NULL
           &&  pCustomTokenDefinition->endComment != NULL
          )
        {
            ++ noTokenDescriptions;
        }
        if(pCustomTokenDefinition->startCommentTillEndOfLine != NULL)
            ++ noTokenDescriptions;

        const size_t sizeOfAry = sizeof(tok_tokenDescriptor_t) * noTokenDescriptions;

        /* We want to have a deep copy of the passed data structure to become decoupled
           from the client's environment. In most use cases the client will use static
           strings (string literals), where this is not really necessary, but it would be
           hard to explain what's permitted and what not, if we wouldn't do so. */
        tok_tokenDescriptor_t * const tokDescAry = smalloc(sizeOfAry, __FILE__, __LINE__);

        unsigned int u;
        const tok_tokenDescriptor_t *pTokDescIn = pCustomTokenDefinition->tokenDescriptorAry;
        tok_tokenDescriptor_t *pTokDescCpy = tokDescAry;
        for(u=0; u<pCustomTokenDefinition->noTokenDescriptions; ++u)
        {
            assert(pTokDescIn->symbol != NULL  &&  pTokDescIn->symbol[0] != '\0');
            pTokDescCpy->symbol = stralloccpy(pTokDescIn->symbol);
            
            assert(pTokDescIn->type < tok_tokenTypeUnidentified 
                   ||  pTokDescIn->type > tok_tokenType_lastTokenOfBuildInList
                  );
            pTokDescCpy->type = pTokDescIn->type;
            
            ++ pTokDescIn;
            ++ pTokDescCpy;
        }

        /* Associate the symbols that form the beginning of a comment with the reserved,
           according token values. */
        if(pCustomTokenDefinition->startComment != NULL
           &&  pCustomTokenDefinition->endComment != NULL
          )
        {
            assert(pCustomTokenDefinition->startComment[0] != '\0'
                   &&  pCustomTokenDefinition->endComment[0] != '\0'
                  );
            pTokDescCpy->symbol = stralloccpy(pCustomTokenDefinition->startComment);
            pTokDescCpy->type = tok_tokenTypeCommentOpener;
            
            /* A copy by reference of the string can be assigned to the field startComment
               - although this is actually not used. */
            pTokenStream->tokenDescriptorTable.startComment = pTokDescCpy->symbol;
            
            /* The end tag is required but not as part of the sorted array of tokens. */
            pTokenStream->tokenDescriptorTable.endComment =
                                            stralloccpy(pCustomTokenDefinition->endComment);
                          
            ++ pTokDescCpy;
        }
        else
        {
            assert(pCustomTokenDefinition->startComment == NULL
                   &&  pCustomTokenDefinition->endComment == NULL
                  );
            pTokenStream->tokenDescriptorTable.startComment = NULL;
            pTokenStream->tokenDescriptorTable.endComment = NULL;
        }

        /* The same for the second comment style. */
        if(pCustomTokenDefinition->startCommentTillEndOfLine != NULL)
        {
            assert(pCustomTokenDefinition->startCommentTillEndOfLine[0] != '\0');
            pTokDescCpy->symbol =
                           stralloccpy(pCustomTokenDefinition->startCommentTillEndOfLine);
            pTokDescCpy->type = tok_tokenTypeCommentTillEndOfLine;
            
            /* A copy by reference of the string can be assigned to the field
               startCommentTillEndOfLine - although this is actually not used. */
            pTokenStream->tokenDescriptorTable.startCommentTillEndOfLine = pTokDescCpy->symbol;

            ++ pTokDescCpy;
        }
        else
            pTokenStream->tokenDescriptorTable.startCommentTillEndOfLine = NULL;
            
        /* The algorithm that matches the characters of the input stream against the
           symbols requires a sort order in rising lexical order on the symbols. */
        qsort( tokDescAry
             , noTokenDescriptions
             , sizeof(tok_tokenDescriptor_t)
             , cmpTokenWithToken
             );

        pTokenStream->tokenDescriptorTable.noTokenDescriptions = noTokenDescriptions;
        pTokenStream->tokenDescriptorTable.tokenDescriptorAry = tokDescAry;
    }
    else
    {
        /* No custom tokens, set everything to null/NULL. */
        pTokenStream->tokenDescriptorTable.noTokenDescriptions = 0;
        pTokenStream->tokenDescriptorTable.tokenDescriptorAry = NULL;
        pTokenStream->tokenDescriptorTable.startComment = NULL;
        pTokenStream->tokenDescriptorTable.endComment = NULL;
        pTokenStream->tokenDescriptorTable.startCommentTillEndOfLine = NULL;
    }
} /* End of createTokenDescriptorTable */




/**
 * Counterpart to createTokenDescriptorTable: The token descritor table is deleted, all
 * malloc allocated strings are freed. The function is used from inside the destructor of a
 * token stream object.
 *   @param pTokenStream
 * The pointer to the currently deleted token stream object.
 */

static void deleteTokenDescriptorTable(tok_tokenStream_t * const pTokenStream)
{
    const unsigned int noTokenDescriptions = pTokenStream->tokenDescriptorTable
                                                           .noTokenDescriptions;

    if(pTokenStream->tokenDescriptorTable.tokenDescriptorAry != NULL)
    {
        unsigned int u;
        tok_tokenDescriptor_t *pTokDesc =
                (tok_tokenDescriptor_t*)pTokenStream->tokenDescriptorTable.tokenDescriptorAry;
        for(u=0; u<noTokenDescriptions; ++u)
        {
            free((char*)pTokDesc->symbol);
            pTokDesc->symbol = NULL;
            ++ pTokDesc;
        }
        free((tok_tokenDescriptor_t*)pTokenStream->tokenDescriptorTable.tokenDescriptorAry);
        pTokenStream->tokenDescriptorTable.tokenDescriptorAry = NULL;
    }

    pTokenStream->tokenDescriptorTable.noTokenDescriptions = 0;

    if(pTokenStream->tokenDescriptorTable.startComment != NULL)
    {
        /* No free, this was just a copy by reference. Invalidate it. */
        pTokenStream->tokenDescriptorTable.startComment = NULL;
    }
    if(pTokenStream->tokenDescriptorTable.endComment != NULL)
    {
        free((char*)pTokenStream->tokenDescriptorTable.endComment);
        pTokenStream->tokenDescriptorTable.endComment = NULL;
    }
    if(pTokenStream->tokenDescriptorTable.startCommentTillEndOfLine != NULL)
    {
        /* No free, this was just a copy by reference. Invalidate it. */
        pTokenStream->tokenDescriptorTable.startCommentTillEndOfLine = NULL;
    }
} /* End of deleteTokenDescriptorTable */




/**
 * A token stream object is created. It is associated with either a stdio stream or with
 * any kind of custom character stream. The created object can be used to parse the input
 * from that stream into a sequence of lexcical atoms or tokens.
 *   @return
 * True, if the stream could be opened successfully, or false otherwise. The only other
 * operation, which is permitted to be applied to the returned object, is
 * tok_deleteTokenStream if the function returns false.
 *   @param phTokenStream
 * The handle to the created object is placed into \a phTokenStream.
 *   @param pErrorString
 * A pointer to a string variable owned by the caller. If an error appears, then a malloc
 * allocated read-only string is placed into this variable. Normally NULL is returned in *
 * \a pErrorString.\n
 *   Caution: The caller is in charge to free the string after use.
 *   @param fileName
 * The name of the file (or character stream), which is parsed. Mainly used for logging.
 * Only, if hFile is NULL then it really needs to be the name of (and path to) an existing
 * file. This file will then be opened for read access and be parsed.\n
 *   If there's no reasonable name (e.g. in case of custom character input) then pass NULL
 * or the empty string.
 *   @param hStream
 * The parsed stream object. Either a stream from the stdio library or a custom stream of
 * unknown type.\n
 *   If parameter \a customGetChar is not NULL, then \a hStream is interpreted as handle of
 * the custom stream. Otherwise it is interpreted as stdio stream (FILE*). In the latter
 * case NULL may be passed: A file \a fileName will be opened as stdio stream and will be
 * parsed then.
 *   @param customFctGetChar
 * The pointer to a custom character input function, which reads a single character from a
 * custom stream of type tok_hCharInputStream_t. Pass NULL if the stdio input should be
 * used.
 *   @param pCustomTokenDefinition
 * The customer provided extension of the internal symbol definitions. Basically a table
 * associating symbols with token values. The tokenizer will look for the symbols in the
 * input stream and return the associated token value if finds one. Additionally the
 * symbols are specified, which mark beginning and end of comments.\n
 *   Pass NULL if no custom symbols are needed.\n
 *   The constructor makes a deep copy of the passed objects. All contained strings can be
 * deleted after return from the constructor.
 */

boolean tok_createTokenStream( tok_hTokenStream_t * const phTokenStream
                             , char * * const pErrorString
                             , const char * const fileName
                             , tok_hStream_t hStream
                             , const tok_customFctGetChar customFctGetChar
                             , const tok_tokenDescriptorTable_t * const pCustomTokenDefinition
                             )
{
#ifdef  DEBUG
    /* Check if patch of snprintf is either not required or properly installed. */
    {
        char buf[3] = {[2] = '\0'};
        snprintf(buf, 2, "%s World", "Hello");
        assert(strlen(buf) == 1);
    }
#endif

    boolean useStdio = customFctGetChar == NULL;

    /* For stdio: We need to see either a file handle or a valid name. */
    assert(!useStdio ||  hStream.hFile != NULL  || (fileName != NULL && *fileName != '\000'));

    /* Most likely errors result from file open operations, therefore do this prior to
       allocating memory space for the object. */
    boolean doOpenFile = useStdio && hStream.hFile == NULL;
    if(doOpenFile)
    {
        hStream.hFile = fopen(fileName, "r");
        if(hStream.hFile == NULL)
        {
            *phTokenStream = TOK_HANDLE_TO_INVALID_TOKEN_STREAM;
            
            /* Retrieve the file error from the stdio lib. */
#define ERR_MSG_FMT_STR "Can't open input file %s (errno: %d, %s)"
            const char *errStdio = strerror(errno);
            const size_t lenErrMsg =  sizeof(ERR_MSG_FMT_STR) + sizeof("-2147483648")
                                      + strlen(fileName)
                                      + strlen(errStdio);
            *pErrorString = smalloc(lenErrMsg, __FILE__, __LINE__);
            snprintf(*pErrorString, lenErrMsg, ERR_MSG_FMT_STR, fileName, errno, errStdio);
            return false;
#undef ERR_MSG_FMT_STR
        }
    }

    tok_tokenStream_t * const pTokenStream = smalloc( sizeof(tok_tokenStream_t)
                                                    , __FILE__
                                                    , __LINE__
                                                    );
    if(fileName != NULL)
        pTokenStream->fileName = stralloccpy(fileName);
    else
        pTokenStream->fileName = stralloccpy("");
    pTokenStream->line = 1;
    pTokenStream->error = false;
    pTokenStream->errorMsg = stralloccpy("");
    pTokenStream->hStream = hStream;
    pTokenStream->getc = customFctGetChar;

    /* Make a deep copy of the client's symbol definitions into this new token stream
       object. */
    createTokenDescriptorTable(pTokenStream, pCustomTokenDefinition);

    pTokenStream->options.eolIsWhiteSpaceOnly = false;
    pTokenStream->options.binLiteral = false;
    pTokenStream->options.suffixMultipliers = false;
    pTokenStream->options.escapeChar = '\\';
    pTokenStream->options.stringQuote = '\"';

    pTokenStream->incLine = false;
    pTokenStream->peeked = false;
    pTokenStream->peekChar = EOF;
    pTokenStream->currentChar = EOF;

    /* The FIFO object is created with a block size that will most probably avoid all
       dynamic allocation and freeing operations. */
    pTokenStream->hFifoChar = fio_createFifoChar(/* blockSize */ 1000);

    /* Provide first character. */
    nextChar(pTokenStream);

    *phTokenStream = pTokenStream;
    *pErrorString = NULL;
    return true;

} /* End of tok_createTokenStream */






/**
 * Delete a token stream object as created by tok_createTokenStream. A possibly open stdio
 * stream is closed.
 *   @param hTokenStream
 * The handle to the object to delete.
 */

void tok_deleteTokenStream(tok_hTokenStream_t hTokenStream)
{
    if(hTokenStream == TOK_HANDLE_TO_INVALID_TOKEN_STREAM)
        return;

    if(hTokenStream->getc == NULL  &&  hTokenStream->hStream.hFile != NULL)
        fclose(hTokenStream->hStream.hFile);

    assert(hTokenStream->hFifoChar != NULL
           &&  fio_getNoElements(hTokenStream->hFifoChar) == 0
          );
    fio_deleteFifoChar(hTokenStream->hFifoChar);

    /* Delete the table of custom symbol definitions. */
    deleteTokenDescriptorTable(hTokenStream);

    assert(hTokenStream->fileName != NULL  &&  hTokenStream->errorMsg != NULL);
    free((char*)hTokenStream->fileName);
    free((char*)hTokenStream->errorMsg);
    free(hTokenStream);

} /* End of tok_deleteTokenStream */




/**
 * Set an option of the tokenizer. Do this after object creation but prior to the first
 * invocation of boolean tok_getNextToken(tok_hTokenStream_t, tok_token_t * const).\n
 *   See enumeration \a tok_syntaxOption_t for the available options.
 *   @param hTStream
 * The handle of the token stream object.
 *   @param option
 * The selected option.
 *   @param value
 * The value to set.
 */

void tok_setBoolOption(tok_hTokenStream_t hTStream, tok_syntaxOption_t option, boolean value)
{
    switch(option)
    {
    case tok_optionEolIsWhiteSpaceOnly:
        hTStream->options.eolIsWhiteSpaceOnly = value;
        break;

    case tok_optionBinLiteral:
        hTStream->options.binLiteral = value;
        break;

    case tok_optionSuffixMultipliers:
        hTStream->options.suffixMultipliers = value;
        break;

    case tok_optionStringsUseSingleQuotes:
        if(value)
            hTStream->options.stringQuote = '\'';
        else
            hTStream->options.stringQuote = '\"';
        break;

    case tok_optionEscapeCharIsDollarSign:
        if(value)
            hTStream->options.escapeChar = '$';
        else
            hTStream->options.escapeChar = '\\';
        break;

    default: assert(false);
    }
} /* End of tok_setBoolOption */




/**
 * Get the current, last recently set error message.
 *   @return
 * Get a read-only string. The returned pointer is only valid until the next invocation of
 * any method on the token stream object.
 *   @param hTStream
 * The handle of the token stream object.
 *   @see boolean tok_resetError(tok_hTokenStream_t, const char ** const)
 */

const char *tok_getErrorMsg(tok_hTokenStream_t hTStream)
{
    return hTStream->errorMsg;

} /* End of tok_getErrorMsg */




/**
 * Acknwledge an error. Most often, a parser will try to continue the process even after an
 * error. If it reported an error from the tokenizer, it should explicitly reset the error
 * in the token stream prior to calling tok_getNextToken the next time. Otherwise it looses
 * the chance to see new errors.
 *   @return
 * The cleared error status is returned.
 *   @param hTStream
 * The handle of the token stream object.
 *   @param pErrMsg
 * If not NULL the current (now cleared) error message is placed into * \a pErrMsg. The
 * returned value is the pointer to a temporary read only string. It is valid only until
 * the next call of whatever method of the token stream object.
 *   @see const char *tok_getErrorMsg(tok_hTokenStream_t)
 */

boolean tok_resetError(tok_hTokenStream_t hTStream, const char * * const pErrMsg)
{
    boolean curErr = hTStream->error;

    hTStream->error = false;
    if(pErrMsg != NULL)
        *pErrMsg = hTStream->errorMsg;

    return curErr;

} /* End of tok_resetError */




/**
 * Get the current line number of the input stream.
 *   @return
 * Get the line number.
 *   @param hTStream
 * The handle of the token stream object.
 */

unsigned int tok_getLine(tok_hTokenStream_t hTStream)
{
    return hTStream->line;

} /* End of tok_getLine */



#if TOK_COMPILE_FPRINTF_TOKEN != 0
/**
 * Debug function (compiled only on demand, see #TOK_COMPILE_FPRINTF_TOKEN):\n
 *   The type and value of a token is printed into a stdio stream.
 *   @param hTStream
 * The handle of the token stream object.
 *   @param hStream
 * A handle to a stdio file with write access in text mode.
 *   @param pToken
 * The token is passed in by reference.
 */

void tok_fprintfToken( tok_hTokenStream_t hTStream
                     , FILE *hStream
                     , const tok_token_t * const pToken
                     )
{
    switch(pToken->type)
    {
    case tok_tokenTypeEndOfLine:
        fprintf(hStream, "End of line");
        break;

    case tok_tokenTypeEndOfFile:
        fprintf(hStream, "End of file");
        break;

    case tok_tokenTypeString:
        fprintf(hStream, "String, \"%s\"", pToken->value.string);
        break;

    case tok_tokenTypeInteger:
        fprintf(hStream, "Integer, %lu", pToken->value.integer);
        break;

    case tok_tokenTypeFpn:
        fprintf(hStream, "Floating point number, %g", pToken->value.fpn);
        break;

    case tok_tokenTypeIdentifier:
        fprintf(hStream, "Identifier, %s", pToken->value.identifier);
        break;

    case tok_tokenTypeCharacter:
        fprintf( hStream, "Character constant, %u ('%c')"
               , (unsigned)pToken->value.character
               , pToken->value.character >= ' '
                 ? pToken->type
                 : '?'
               );
        break;

    case tok_tokenTypeEmptyInitialized:
        fprintf(hStream, "(just initialized, never used token object)");
        break;
        
    case tok_tokenTypeUnidentified:
    case tok_tokenTypeCommentOpener:
    case tok_tokenTypeCommentTillEndOfLine:
    case tok_tokenType_lastTokenOfBuildInList:
        /* These must never be seen! */
        assert(false);
        break;
        
    default:
        /* We either have a character-itself token or a custom symbol - or an error. */
        if(pToken->type < 256)
        {
            fprintf( hStream
                   , "Character-as-token, %u ('%c')"
                   , pToken->type
                   , pToken->type >= ' '  &&  pToken->type < 128? pToken->type: '?'
                   );
        }
        else if(pToken->type >= tok_tokenType_firstCustomToken)
        {
            /* Here we need to do a linear, not aborted search, as the tabel is not sorted
               with respect to the token values and as there might be several symbols
               having the same token value. */
            unsigned int u;
#ifdef DEBUG
            boolean found = false;
#endif
            for(u=0; u<hTStream->tokenDescriptorTable.noTokenDescriptions; ++u)
            {
                if(pToken->type == hTStream->tokenDescriptorTable.tokenDescriptorAry[u].type)
                {
#ifdef DEBUG
                    found = true;
#endif
                    fprintf( hStream
                           , "Custom symbol, %s"
                           , hTStream->tokenDescriptorTable.tokenDescriptorAry[u].symbol
                           );
                }
            }
            assert(found);
        }
        else
            assert(false);
    }
} /* End of tok_fprintfToken */
#endif



/**
 * Main operation on a token stream: Read the next token (or lexical atom) from the input
 * stream.\n
 *   The end-of-file is always defined as a token. The client of this class will call this
 * method in a loop until it gets the token EOF. Further calls are permitted but will
 * always return EOF again.
 *   @return
 * Recognizing tokens in read character sequences can fail. If the next characters in the
 * stream do not form any defined token, then the function returns false. The client knows
 * that there is a syntax error. It can request the error message by const char
 * *tok_getErrorMsg(tok_hTokenStream_t).
 *   @param hTStream
 * The handle of the token stream object to read from.
 *   @param pToken
 * The read token is placed into * \a pToken.
 */

boolean tok_getNextToken(tok_hTokenStream_t hTStream, tok_token_t * const pToken)
{
    /* The outer loop reads tokens as long as it is not a comment - comment tokens are not
       returned to the client. */
    do
    {
        /* The last read character of the previous invocation of this routine had been consumed
           from the input stream in order to find the end of the previously returned token.
           This character is now the starting character of the token to be read this time.
           Retrieve it without reading from the input stream again. */
        signed int ch = currentChar(hTStream);

        /* Strings and identifiers are returned to the client as malloc allocated strings.
           The client can take ownership of these strings by replacing the pointers with
           NULL. In all other cases we return the strings now to the heap.
             A pointer can't be checked for pointing to a malloc allocated chunk of memory.
           Therefore this strategy is somewhat unsafe. If the client passes a token object
           in, which is neither properly initialized nor filled by a preceding call of this
           function, then we will probably free a string, which was not a result of malloc
           - with probably catastrophic consequences. */
        if(pToken->type == tok_tokenTypeString  &&  pToken->value.string != NULL )
        {
           free((char*)pToken->value.string);
           pToken->value.string = NULL;
        }
        else if(pToken->type == tok_tokenTypeIdentifier  &&  pToken->value.identifier != NULL)
        {
           free((char*)pToken->value.identifier);
           pToken->value.identifier = NULL;
        }

        /* Read next character, but skip blank space. The end of a line is not considered
           white space as it is most important to many parsers. */
        while(!hTStream->error &&  iswhitespace(hTStream, ch))
            ch = nextChar(hTStream);

        /* First we handle the simple cases: Tokens which can be safely identified because
           of their first character. */

        /* The string always begins with a (double) quote. */
        if(ch == optionStringQuote(hTStream))
        {
            readString(hTStream, pToken);

        } /* End of reading a string. */


        /* A numeric literal always begins with a digit or with a dot followed by a digit.
           It's still undecided whether we see an integer or a floating point number.
             A number never has a sign, the sign will be scanned as a separate token and
           needs to be handled by the parser, which is client of this tokenizer. */
        else if(isdigit(ch)
                || (ch == '.'  &&  isdigit(peekChar(hTStream)))
                || (boolOption(hTStream, tok_optionBinLiteral)
                    &&  ch == '%'  &&  strchr("01", peekChar(hTStream)) != NULL
                   )
               )
        {
            readNumeral(hTStream, pToken);

        } /* End of reading a number literal. */


        /* The next trivial to recognize lexical atom is the identifier, which is defined
           as in C. It could however also be a client defined keyword. */
        else if( isalpha(ch) || ch=='_' )
        {
            readIdentifier(hTStream, pToken);

        } /* End of reading an identifier. */


        /* The last trivial to recognize syntax element: The character constant. It is
           recognized with the opening quote.
             It's an option to define the quotes for strings to be single quotes. If this
           option is chosen, then character constants are not defined. */
        else if(!boolOption(hTStream, tok_optionStringsUseSingleQuotes) &&  ch == '\'')
        {
            readCharacterConstant(hTStream, pToken);
            
        } /* End of reading a character constant. */


        /* Now we have the tokens, which can't be recognized at their first character. They
           are defined in the client provided table. */
        else
        {
            /* The next call will always decide on a token type because the final fallback
               decision is to return the single character itself as a token. */
            readCustomSymbol(hTStream, pToken);

            /* Special handling is needed if we recognized the symbol, which is defined to
               be the beginning of a comment. The contents of a comment cannot be scanned
               like the rest of the input, as no formal syntax can be assumed for these
               contents. We simply read the input until we see the end defined for the
               comment.
                 Two kinds of comments are defined: Ended by a counterpart symbol or ended
               by the next end of line character (which is defined to not belong to the
               comment). */
            if(!hTStream->error
               && (pToken->type == tok_tokenTypeCommentOpener
                   ||  pToken->type == tok_tokenTypeCommentTillEndOfLine
                  )
              )
            {
                /* Read till the end of the comment (including) but do not change the found
                   token - the comment's contents are not returned. */
                readComment(hTStream, pToken);

            } /* End if(Comment opener symbol recognized?) */

        } /* End if(Can we match the input with a client defined symbol?) */
    }
    while(!hTStream->error
          &&  (pToken->type == tok_tokenTypeCommentOpener
               ||  pToken->type == tok_tokenTypeCommentTillEndOfLine
              )
         );

    return !hTStream->error;

} /* End of tok_getNextToken */




