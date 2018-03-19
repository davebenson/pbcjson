/* This is a validator / scanner that will give find references to the start/end of
 * a record while ensuring that the contents are valid UTF-8,
 * modulo the flags which offers all types of non-conformance.
 */

#ifndef __JSON_VALIDATOR_H_
#define __JSON_VALIDATOR_H_

// This macro is available to make all the functions static.
// Which is what we do in pbcjson since i don't want to expose these symbols;
// we simply:
//       #define JSON_VALIDATOR_FUNC  static
//       #define "json-validator.c"
//
#ifndef JSON_VALIDATOR_FUNC
#define JSON_VALIDATOR_FUNC
#endif

typedef struct JSON_Validator_Options JSON_Validator_Options;
typedef struct JSON_Validator JSON_Validator;

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  JSON_VALIDATOR_ENCAPSULATION_WHITESPACE_SEP,    /* default */
  JSON_VALIDATOR_ENCAPSULATION_COMMA_SEP,
  JSON_VALIDATOR_ENCAPSULATION_LINE_BY_LINE,      /* possibly optimized */
  JSON_VALIDATOR_ENCAPSULATION_ARRAY,
} JSON_Validator_Encapsulation;
        
struct JSON_Validator_Options {
  unsigned max_stack_depth;

  // These flags are chosen so that 0 is the strict/standard JSON interpretation.
  unsigned ignore_utf8_errors : 1;
  unsigned ignore_utf16_errors : 1;
  unsigned ignore_utf8_surrogate_pairs : 1;
  unsigned permit_trailing_commas : 1;
  unsigned ignore_comments : 1;
  unsigned allow_bare_fieldnames : 1;
  unsigned permit_single_quote_strings : 1;
  unsigned permit_leading_decimal_numbers : 1;
  unsigned disallow_extra_whitespace : 1;
  unsigned permit_line_continuations_in_strings : 1;

  // These values describe the encapsulation of the JSON records
  unsigned permit_bare_values : 1;
  unsigned permit_array_values : 1;
  unsigned permit_toplevel_commas : 1;

  // default is line-by-line which is my recommended encapsulation
  JSON_Validator_Encapsulation encapsulation;

  // Used for error information.
  unsigned start_line_number;
};


typedef enum
{
  JSON_VALIDATOR_RESULT_INTERIM,      // spacing etc in between records
  JSON_VALIDATOR_RESULT_IN_RECORD,    // record did not come to an end (*used==len)
  JSON_VALIDATOR_RESULT_RECORD_ENDED, // spacing etc in between records
  JSON_VALIDATOR_RESULT_ERROR         // call json_validator_get_error_info()
} JSON_ValidatorResult;
  
typedef enum
{
  JSON_VALIDATOR_ERROR_NONE,

  JSON_VALIDATOR_ERROR_EXPECTED_DOUBLE_QUOTE,
  JSON_VALIDATOR_ERROR_EXPECTED_COLON,
  JSON_VALIDATOR_ERROR_EXPECTED_VALUE,
  JSON_VALIDATOR_ERROR_EXPECTED_COMMA_OR_RBRACKET,
  JSON_VALIDATOR_ERROR_EXPECTED_COMMA_OR_RBRACE,
  JSON_VALIDATOR_ERROR_EXPECTED_ARRAY_START,
  JSON_VALIDATOR_ERROR_EXTRA_COMMA,
  JSON_VALIDATOR_ERROR_BAD_NUMBER,
  JSON_VALIDATOR_ERROR_STACK_DEPTH_EXCEEDED,

  // errors that can occur while validating a string.
  JSON_VALIDATOR_ERROR_UTF8_OVERLONG,
  JSON_VALIDATOR_ERROR_UTF8_BAD_INITIAL_BYTE,
  JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE,
  JSON_VALIDATOR_ERROR_UTF8_SURROGATE_PAIR_NOT_ALLOWED,
  JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR,
  JSON_VALIDATOR_ERROR_STRING_CONTROL_CHARACTER,
  JSON_VALIDATOR_ERROR_QUOTED_NEWLINE,
  JSON_VALIDATOR_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL,
} JSON_ValidatorError;

typedef enum
{
  JSON_VALIDATOR_FLAT_ERROR_NONE,
} JSON_Validator_FlatError;

//---------------------------------------------------------------------
//                           Functions
//---------------------------------------------------------------------
JSON_VALIDATOR_FUNC
size_t
json_validator_options_get_memory_size (const JSON_Validator_Options *options);

// note thtat JSON_Validator allocates no further memory, so there
JSON_VALIDATOR_FUNC
JSON_Validator *
json_validator_init (void *memory,
                     const JSON_Validator_Options *options);
 


JSON_VALIDATOR_FUNC
JSON_ValidatorResult
json_validator_feed (JSON_Validator *validator,
                     size_t          len,
                     const uint8_t  *data,
                     size_t         *used);


JSON_VALIDATOR_FUNC
JSON_ValidatorResult
json_validator_end_feed (JSON_Validator *validator);


JSON_VALIDATOR_FUNC
JSON_ValidatorError
json_validator_get_error_info(JSON_Validator *validator);

#endif /*__JSON_VALIDATOR_H_*/
