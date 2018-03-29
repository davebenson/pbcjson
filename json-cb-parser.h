/* Sort-of a SAX-type parser for JSON.
 */

#ifndef __JSON_CALLBACK_PARSER_H_
#define __JSON_CALLBACK_PARSER_H_

// This macro is available to make all the functions static.
// Which is what we do in pbcjson since i don't want to expose these symbols;
// we simply:
//       #define JSON_CALLBACK_PARSER_FUNC_DECL  static
//       #define "json-callback_parser.c"
//
#ifndef JSON_CALLBACK_PARSER_FUNC_DECL
#define JSON_CALLBACK_PARSER_FUNC_DECL
#endif
#ifndef JSON_CALLBACK_PARSER_FUNC_DEF
#define JSON_CALLBACK_PARSER_FUNC_DEF  JSON_CALLBACK_PARSER_FUNC_DECL
#endif

typedef struct JSON_CallbackParser_Options JSON_CallbackParser_Options;
typedef struct JSON_CallbackParser JSON_CallbackParser;

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define JSON_CALLBACK_PARSER_DEFAULT_MAX_DEPTH 64

struct JSON_CallbackParser_Options {
  unsigned max_stack_depth;

  // These flags are chosen so that 0 is the strict/standard JSON interpretation.
  unsigned ignore_utf8_errors : 1;              // UNIMPL
  unsigned ignore_utf16_errors : 1;             // UNIMPL
  unsigned ignore_utf8_surrogate_pairs : 1;     // UNIMPL
  unsigned permit_backslash_x : 1;
  unsigned permit_backslash_0 : 1;
  unsigned permit_trailing_commas : 1;
  unsigned ignore_single_line_comments : 1;
  unsigned ignore_multi_line_comments : 1;
  unsigned ignore_multiple_commas : 1;
  unsigned ignore_missing_commas : 1;
  unsigned permit_bare_fieldnames : 1;
  unsigned permit_single_quote_strings : 1;
  unsigned permit_leading_decimal_point : 1;
  unsigned permit_hex_numbers : 1;
  unsigned permit_octal_numbers : 1;
  unsigned disallow_extra_whitespace : 1;       // brutal non-conformant optimization (NOT IMPL)
  unsigned ignore_unicode_whitespace : 1;
  unsigned permit_line_continuations_in_strings : 1;

  // These values describe the encapsulation of the JSON records
  unsigned permit_bare_values : 1;
  unsigned permit_array_values : 1;
  unsigned permit_toplevel_commas : 1;

  // Used for error information.
  unsigned start_line_number;
};

extern JSON_CallbackParser_Options json_callback_parser_options_json;
extern JSON_CallbackParser_Options json_callback_parser_options_json5;
#define JSON_CALLBACK_PARSER_OPTIONS_INIT                     \
(JSON_CallbackParser_Options) {                               \
  .max_stack_depth = JSON_CALLBACK_PARSER_DEFAULT_MAX_DEPTH,  \
  .ignore_utf8_errors = 0,                                    \
  .ignore_utf16_errors = 0,                                   \
  .ignore_utf8_surrogate_pairs = 0,                           \
  .permit_backslash_x = 0,                                    \
  .permit_backslash_0 = 0,                                    \
  .permit_trailing_commas = 0,                                \
  .ignore_single_line_comments = 0,                           \
  .ignore_multi_line_comments = 0,                            \
  .ignore_multiple_commas = 0,                                \
  .ignore_missing_commas = 0,                                 \
  .permit_bare_fieldnames = 0,                                \
  .permit_single_quote_strings = 0,                           \
  .permit_leading_decimal_point = 0,                          \
  .permit_hex_numbers = 0,                                    \
  .permit_octal_numbers = 0,                                  \
  .disallow_extra_whitespace = 0,                             \
  .ignore_unicode_whitespace = 0,                             \
  .permit_line_continuations_in_strings = 0,                  \
  .permit_bare_values = 0,                                    \
  .permit_array_values = 0,                                   \
  .permit_toplevel_commas = 0,                                \
  .start_line_number = 1,                                     \
}

typedef enum
{
  JSON_CALLBACK_PARSER_ERROR_NONE,

  JSON_CALLBACK_PARSER_ERROR_EXPECTED_MEMBER_NAME,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_COLON,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_VALUE,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_COMMA_OR_RBRACKET,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_COMMA_OR_RBRACE,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_HEX_DIGIT,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_STRUCTURED_VALUE,
  JSON_CALLBACK_PARSER_ERROR_EXPECTED_COMMA,
  JSON_CALLBACK_PARSER_ERROR_EXTRA_COMMA,
  JSON_CALLBACK_PARSER_ERROR_TRAILING_COMMA,
  JSON_CALLBACK_PARSER_ERROR_SINGLE_QUOTED_STRING_NOT_ALLOWED,
  JSON_CALLBACK_PARSER_ERROR_STACK_DEPTH_EXCEEDED,
  JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR,
  JSON_CALLBACK_PARSER_ERROR_BAD_BAREWORD,
  JSON_CALLBACK_PARSER_ERROR_PARTIAL_RECORD,          // occurs at unexpected EOF

  // errors that can occur while validating a string.
  JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG,
  JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_INITIAL_BYTE,
  JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE,
  JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR,
  JSON_CALLBACK_PARSER_ERROR_STRING_CONTROL_CHARACTER,
  JSON_CALLBACK_PARSER_ERROR_QUOTED_NEWLINE,
  JSON_CALLBACK_PARSER_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL,
  JSON_CALLBACK_PARSER_ERROR_BACKSLASH_X_NOT_ALLOWED,

  // errors that can occur while validating a number
  JSON_CALLBACK_PARSER_ERROR_HEX_NOT_ALLOWED,
  JSON_CALLBACK_PARSER_ERROR_OCTAL_NOT_ALLOWED,
  JSON_CALLBACK_PARSER_ERROR_NON_OCTAL_DIGIT,
  JSON_CALLBACK_PARSER_ERROR_LEADING_DECIMAL_POINT_NOT_ALLOWED,
  JSON_CALLBACK_PARSER_ERROR_TRAILING_DECIMAL_POINT_NOT_ALLOWED,
  JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER,

  JSON_CALLBACK_PARSER_ERROR_INTERNAL,

} JSON_CallbackParserError;

typedef struct {
  JSON_CallbackParserError code;
  const char *code_str;
  uint64_t line_no;
  uint64_t byte_no;
  const char *message;
  const char *message2;         /// may be NULL
} JSON_CallbackParser_ErrorInfo;

//---------------------------------------------------------------------
//                           Functions
//---------------------------------------------------------------------
typedef struct JSON_Callbacks JSON_Callbacks;
struct JSON_Callbacks {
  bool (*start_object)  (void *callback_data);
  bool (*end_object)    (void *callback_data);
  bool (*start_array)   (void *callback_data);
  bool (*end_array)     (void *callback_data);
  bool (*object_key)    (unsigned key_length,
                         const char *key,
                         void *callback_data);
  bool (*number_value)  (unsigned number_length,
                         const char *number,
                         void *callback_data);
  unsigned (*partial_string_value)
                        (unsigned cur_string_length_in_bytes,
                         const char *cur_string,
                         void *callback_data);
  bool (*string_value)  (unsigned number_length,
                         const char *number,
                         void *callback_data);
  bool (*boolean_value) (int boolean_value,
                       void *callback_data);
  bool (*null_value)    (void *callback_data);

  bool (*error)         (const JSON_CallbackParser_ErrorInfo *error,
                         void *callback_data);

  void (*destroy)       (void *callback_data);
};
#define JSON_CALLBACKS_DEF(prefix, suffix) \
  {                                        \
    prefix ## start_object ## suffix,      \
    prefix ## end_object   ## suffix,      \
    prefix ## start_array  ## suffix,      \
    prefix ## end_array    ## suffix,      \
    prefix ## object_key   ## suffix,      \
    prefix ## number_value ## suffix,      \
    prefix ## partial_string_value ## suffix,\
    prefix ## string_value ## suffix,      \
    prefix ## boolean_value## suffix,      \
    prefix ## null_value   ## suffix,      \
    prefix ## error        ## suffix,      \
    prefix ## destroy      ## suffix       \
  }
// note thtat JSON_CallbackParser allocates no further memory, so there
JSON_CALLBACK_PARSER_FUNC_DECL
JSON_CallbackParser *
json_callback_parser_new (const JSON_Callbacks *callbacks,
                          void *callback_data,
                          const JSON_CallbackParser_Options *options);
 


JSON_CALLBACK_PARSER_FUNC_DECL
bool
json_callback_parser_feed (JSON_CallbackParser *callback_parser,
                           size_t          len,
                           const uint8_t  *data);


JSON_CALLBACK_PARSER_FUNC_DECL
bool
json_callback_parser_end_feed (JSON_CallbackParser *callback_parser);

JSON_CALLBACK_PARSER_FUNC_DECL
void
json_callback_parser_destroy (JSON_CallbackParser *callback_parser);


#endif /*__JSON_CALLBACK_PARSER_H_*/
