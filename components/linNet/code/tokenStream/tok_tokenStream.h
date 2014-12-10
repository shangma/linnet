#ifndef TOK_TOKENSTREAM_INCLUDED
#define TOK_TOKENSTREAM_INCLUDED
/**
 * @file tok_tokenStream.h
 * Definition of global interface of module tok_tokenStream.c
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

#include "types.h"


/*
 * Defines
 */

/** Customization: Decide whether to compile a debug function, which prints
    the type and value of a token into a stdio stream.\n
      Set the value of the macro to either 0 or 1. */
#define TOK_COMPILE_FPRINTF_TOKEN   1


/** A constant initializer expression, that may be used as RHS of the definition of a
    token stream object. It's an invalid NULL handle, which must not be used at all. */
#define TOK_HANDLE_TO_INVALID_TOKEN_STREAM  NULL


/** A compile time initializer expression, which can be used for objects of type
    tok_token_t. */
#define TOK_UNINITIALIZED_TOKEN { .type = tok_tokenTypeEmptyInitialized                     \
                                , .value = {.identifier = NULL}                             \
                                }

/** This switch decides whether the function tok_getTokenType is compiled. Normally, this
    function will be used for debug and testing purpose only but is not required in the
    production code. Therefore, setting the macro can be done from outside, e.g. controlled
    by the makefile.
      @remark The function tok_getTokenType makes a switch/case of the different known
    token types. As these depent (partly) on the compile time application configuration the
    function implementation may be subject to application specific modifications. */
#ifndef TOK_USE_GET_TOKEN_TYPE
# define TOK_USE_GET_TOKEN_TYPE 1
#endif


/*
 * Global type definitions
 */

/** Basically, the type of a recognized token is an enumeration. In order to let this
    enumeration be extendable at different code locations we implement it not as a true
    enum but as an integer, which we will only assign true enum values to. We use the
    signed type as the constant EOF from the stdio library is an important example of a
    token type. */
typedef signed int tok_tokenType_t;


/** This enumeration lists the hard coded, not application dependent types of symbols, like
    string constants, integer and floating point number literals.\n
      The enumeration starts with the numeric value 256. The scanner will return single
    characters, which do not belong to a known token type as themselves: These characters
    are used as the them decribing token type at the same time.\n
      The list ends with a named value, that actually is not a token type but can be used
    to continue the enumeration at another code location. This is used to define the
    application dependent token definitions.
       @remark The token types \a tok_tokenTypeCommentOpener and \a
    tok_tokenTypeCommentTillEndOfLine are used internally only. If a such a token is
    recognized by the tokenizer it won't return it to the client but will silently discrad
    it and read the next token from the input stream. The first non-comment token is
    returned. */
enum { tok_tokenTypeEndOfLine = (tok_tokenType_t)'\n'
     , tok_tokenTypeEndOfFile = (tok_tokenType_t)EOF
     , tok_tokenTypeUnidentified = 256
     , tok_tokenTypeEmptyInitialized
     , tok_tokenTypeCommentOpener           /// Only used internally
     , tok_tokenTypeCommentTillEndOfLine    /// Only used internally
     , tok_tokenTypeIdentifier
     , tok_tokenTypeInteger
     , tok_tokenTypeFpn
     , tok_tokenTypeCharacter
     , tok_tokenTypeString

     /** Custom token values must be greater than this one. */
     , tok_tokenType_lastTokenOfBuildInList

     /** Client code should use this value to initialize its first custom token vlaue. */
     , tok_tokenType_firstCustomToken
     };


/** The recognized lexical atom is returned by the token stream (function \a
    tok_getNextToken) as an object of this structure. The intended use case is to have a
    single instance of this type and to reuse it throughout the complete tokenizing
    process. In each call of function \a tok_getNextToken it'll be filled with the
    information describing the next lexical atom found in the input stream.\n
      Caution, an object of this type is both, input and output to function \a
    tok_getNextToken. If the token is of kind integer or string, then its value is the
    pointer to a malloc allocated string. This pointer - if not NULL - is freed on entry
    into the function, in order to make room for the value of the next token. Therefore,
    prior to the very first invocation of function \a tok_getNextToken the object needs to
    be properly initialize. The combination of kind integer or string with a non NULL
    pointer value would cause a severe error by freeing this seeming heap object. To avoid
    this problem, always use macro #TOK_UNINITIALIZED_TOKEN as a compile time initializer
    expression for instances of this type.
      @see boolean tok_getNextToken(tok_hTokenStream_t, tok_token_t * const) */
typedef struct tok_token_t
{
    /** The type of the token. */
    tok_tokenType_t type;

    /** Some tokens are rather token classes: They can have many different values. The found
        value is returned in this field. Which one to use depends on the token type. */
    union
    {
        /** If \a type is \a tok_tokenTypeIdentifier then \a value.identifier holds the
            pointer to a malloc allocated string with the name of the identifier. The
            caller may copy the pointer value and set \a identifier to NULL before function
            \a tok_getNextToken is invoced the next time. This way the caller would take
            the ownership of the string and would be in charge to free it after use. */
        const char *identifier;

        /** If \a type is \a tok_tokenTypeInteger then \a value.integer holds the value of
            the (signless) integer literal. */
        unsigned long integer;

        /** If \a type is \a tok_tokenTypeFpn then \a value.fpn holds the value of the
            floating point literal. */
        double fpn;

        /** If \a type is \a tok_tokenTypeCharacter then \a value.character holds the
            numeric value of a C-style character constant. */
        signed char character;

        /** If \a type is \a tok_tokenTypeString then \a value.string holds the pointer to
            a malloc allocated string with the string contents. The caller may copy the
            pointer value and set \a string to NULL before function \a tok_getNextToken is
            invoced the next time. This way the caller would take the ownership of the
            string and would be in charge to free it after use. */
        const char *string;

    } value;

} tok_token_t;


/** The token stream is defined as an anonymous struct. Here, we only defined the handle
    of an object of this class. */
typedef struct tok_tokenStream_t *tok_hTokenStream_t;


/** Some syntax constructs of the tokenizer are optional, i.e. they can be switched on or
    off in order to customize the behavior.\n
      Use the values of this enumeration with method tok_setBoolOption to set the options
    appropriately. */
typedef enum
{
    /** By default, the end of line is a recognized token. This can be switched off
        using this option.\n
          There's an important interrelation with string tokens: As in C, if string
        literals are separated by white space only, then they are seen as one string
        literal. If the EOL character is not seen as white space, then string literal
        melting becomes rather useless as it would take place only with string literals on
        the same line. */
    tok_optionEolIsWhiteSpaceOnly

    /** Binary number literal are supported. They begin with a %, immediately followed by a
        sequence of 1 and 0 digits. Binary numbers are limited to positive integers.
        Default is false. */
    , tok_optionBinLiteral

    /** Floating point numbers can have a one-character exponent, e.g. 1k meaning 1e3 or
        23.4u meaning 23.4e-6. Default is false.\n
          The table of supported suffixes has been taken from
        http://searchstorage.techtarget.com/definition/Kilo-mega-giga-tera-peta-and-all-that,
        visted on Feb 9, 2014. The suffix E for 10^18 had to be changed to X because of
        the conflict with the normal exponential form of a floating point number:\n
          yocto (10^-24): use suffix y\n
          zepto (10^-21): use suffix z\n
          atto  (10^-18): use suffix a\n
          femto (10^-15): use suffix f\n
          pico  (10^-12): use suffix p\n
          nano  (10^-9) : use suffix n\n
          micro (10^-6) : use suffix u\n
          milli (10^-3) : use suffix m\n
          centi (10^-2) : use suffix c\n
          deci  (10^-1) : use suffix d\n
          deka  (10^1)  : use suffix D\n
          hecto (10^2)  : use suffix h\n
          kilo  (10^3)  : use suffix k\n
          mega  (10^6)  : use suffix M\n
          giga  (10^9)  : use suffix G\n
          tera  (10^12) : use suffix T\n
          peta  (10^15) : use suffix P\n
          exa   (10^18) : use suffix X (instead of E)\n
          zetta (10^21) : use suffix Z\n
          yotta (10^24) : use suffix Y */
    , tok_optionSuffixMultipliers

    /** The escape character for special characters in strings can be switched from the
        standard backslash to the alternative $. This is considered useful in Windows
        environments, where starings with the backslash as an ordinary character are quite
        common as file paths. Default is the backslash. */
    , tok_optionEscapeCharIsDollarSign

    /** Some environments define strings as enclosed in single quotes. If this option is
        defined then no character constants are defined (which look as in C, also in single
        quotes); now they are recognized and returned as strings of length one. Default is
        to use double quotes. */
    , tok_optionStringsUseSingleQuotes

} tok_syntaxOption_t;


/** The client of the scanner may use a file from stdio as input and use a built-in read
    function or he might specify his own character read function. In the latter case he
    will define a character input function f as:\n
      signed int f(tok_hCharInputStream_t hStream)\n
    The type of his stream is opaque to this scanner implementation, the client will define
    the type in close relation to his character input function. */
typedef struct tok_charInputStream_t *tok_hCharInputStream_t;


/** The custom character stream has an only mandatory function in its interface. This is
    the function to read the next character from that stream. The function takes the
    stream handle type as input. Given the possibility to hide any kind of data structure
    behind the handle type it should be possible to implement any idea of an input
    stream.\n
      Not apparent in the interface definition but essential: The end of input is signaled
    by returning the constant EOF (defined by the stdio library) once and also over and
    over if the function should continuously be invoked.\n
      Stream errors can npt be reported. If an error appears the custom shall behave as if
    it was the end of the input. */
typedef signed int (*tok_customFctGetChar)(tok_hCharInputStream_t hCustomCharStream);


/** The unification of the different stream types that can be scanned. Either a stream from
    the library stdio or an externally defined custom stream. */
typedef union
{
    /** A stream as defined by stdio. */
    FILE *hFile;

    /** A custom character stream. */
    tok_hCharInputStream_t hCustomStream;

} tok_hStream_t;


/** Internally, the scanner implementation uses a table, that relates recognized, specific
    character sequences to an integer value, which is returned as token type. The client
    may define his own, specific character sequences or symbols or tokens by passing an
    extention of the built-in symbol definitions to the scanner. The elements of this
    extended syntax definition have this type.\n
      @remark Identical symbols are not supported, but currently the implementation doesn't
    double-check for doubly defined smbols.
      @remark Using identical token values (or \a type) is valid and supported. Different
    character sequences would be reported in a transparent way to the client as same token.
    One could e.g. define the \a symbol ";" as \a type \a tok_tokenTypeEndOfLine, in which
    case the semicolon would have the same meaning to the client parser as an end of line
    character. */
typedef struct tok_tokenDescriptor_t
{
   const char *symbol;
   tok_tokenType_t type;

} tok_tokenDescriptor_t;


/** See other type tok_tokenDescriptor_t: Here we have the complete token definition
    table, which contains all client specified symbols and their token values. */
typedef struct tok_tokenDescriptorTable_t
{
    /** The number of client defined symbols, or the number of elements of \a
        tokenDescriptorAry. May be 0 if no custom symbols are required. */
    unsigned int noTokenDescriptions;

    /** The list of all client defined symbols, organized as an array. There's no specific
        sort order for the array.
          @remark: No symbol must be NULL or the empty string and no two symbols must
        be the identical. The latter condition is not checked, the former are in DEBUG
        compilation (by assertion).
          @remark Using identical token values (or \a type) is valid and supported.
        Different character sequences would be reported in a transparent way to the client
        as same token. One could e.g. define the \a symbol ";" as \a type \a
        tok_tokenTypeEndOfLine, in which case the semicolon would have the same meaning to
        the client parser as an end of line character. */
    const tok_tokenDescriptor_t *tokenDescriptorAry;

    /** The tokenizer supports two comment styles. The first one uses two symbols to mark
        beginnning and end of the comment (both including). The second one uses a symbol to
        mark the beginning of the comment (including) and the comment ends at the next end
        of line character (excluding).\n
          Here the client defines the beginning of a comment of the first style.\n
          If no such comment is needed, the value is set to NULL. */
    const char *startComment;

    /** The tokenizer supports two comment styles. The first one uses two symbols to mark
        beginnning and end of the comment (both including). The second one uses a symbol to
        mark the beginning of the comment (including) and the comment ends at the next end
        of line character (excluding).\n
          Here the client defines the end of a comment of the first style.\n
          If no such comment is needed, the value is set to NULL. */
    const char *endComment;

    /** The tokenizer supports two comment styles. The first one uses two symbols to mark
        beginnning and end of the comment (both including). The second one uses a symbol to
        mark the beginning of the comment (including) and the comment ends at the next end
        of line character (excluding).\n
          Here the client defines the beginning of a comment of the second style.\n
          If no such comment is needed, the value is set to NULL. */
    const char *startCommentTillEndOfLine;

} tok_tokenDescriptorTable_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** A token stream object is created. */
boolean tok_createTokenStream( tok_hTokenStream_t * const phTokenStream
                             , char * * const pErrorString
                             , const char * const fileName
                             , tok_hStream_t hStream
                             , const tok_customFctGetChar customFctGetChar
                             , const tok_tokenDescriptorTable_t * const pCustomTokenDefinition
                             );

/** Delete a token stream object as created by tok_createTokenStream. */
void tok_deleteTokenStream(tok_hTokenStream_t hTokenStream);

/** Set a Boolean option of the tokenizer. */
void tok_setBoolOption(tok_hTokenStream_t hTStream, tok_syntaxOption_t option, boolean value);

/** Get the current, last recently set error message. */
const char *tok_getErrorMsg(tok_hTokenStream_t hTStream);

/** Reset error in order to proceed scanning the input. */
boolean tok_resetError(tok_hTokenStream_t hTStream, const char * * const pErrMsg);

/** Get the current line number of the input stream. */
unsigned int tok_getLine(tok_hTokenStream_t hTStream);

#if TOK_COMPILE_FPRINTF_TOKEN != 0
/** Debug function (compiled only on demand, see #TOK_COMPILE_FPRINTF_TOKEN): The type
    and value of a token is printed into a stdio stream. */
void tok_fprintfToken( tok_hTokenStream_t hTStream
                     , FILE *hStream
                     , const tok_token_t * const pToken
                     );
#endif

/** Main operation on a token stream: Read the next token (or lexical atom) from the input. */
boolean tok_getNextToken(tok_hTokenStream_t hTStream, tok_token_t * const pToken);

#endif  /* TOK_TOKENSTREAM_INCLUDED */
