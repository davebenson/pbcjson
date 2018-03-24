#include "json-cb-parser.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


#define IS_SPACE(c)  ((c)=='\t' || (c)=='\r' || (c)==' ' || (c)=='\n')
#define IS_HEX_DIGIT(c)     (('0' <= (c) && (c) <= '9')                 \
                          || ('a' <= (c) && (c) <= 'f')                 \
                          || ('A' <= (c) && (c) <= 'F'))
#define IS_OCT_DIGIT(c)    ('0' <= (c) && (c) <= '7')
#define IS_DIGIT(c)    ('0' <= (c) && (c) <= '9')

#define IS_ASCII_ALPHA(c)   (('a' <= (c) && (c) <= 'z')                 \
                          || ('A' <= (c) && (c) <= 'Z'))
#define IS_ALNUM(c)       (('0' <= (c) && (c) <= '0')                   \
                          || IS_ASCII_ALPHA(c))

#define IS_HI_SURROGATE(u) (0xd800 <= (u) && (u) <= (0xd800 + 2047 - 64))
#define IS_LO_SURROGATE(u) (0xdc00 <= (u) && (u) <= (0xdc00 + 1023))

#define COMBINE_SURROGATES(hi, lo)   (((uint32_t)((hi) - 0xd800) << 10)     \
                                     + (uint32_t)((lo) - 0xdc00) + 0x10000)

typedef enum
{
  JSON_CALLBACK_PARSER_STATE_INTERIM,
  JSON_CALLBACK_PARSER_STATE_INTERIM_EXPECTING_COMMA,
  JSON_CALLBACK_PARSER_STATE_INTERIM_GOT_COMMA,
  JSON_CALLBACK_PARSER_STATE_INTERIM_VALUE,
  JSON_CALLBACK_PARSER_STATE_IN_ARRAY,
  JSON_CALLBACK_PARSER_STATE_IN_ARRAY_EXPECTING_COMMA,
  JSON_CALLBACK_PARSER_STATE_IN_ARRAY_GOT_COMMA,
  JSON_CALLBACK_PARSER_STATE_IN_ARRAY_VALUE,   // flat_value_state is valid
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT,
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT_AWAITING_COLON,
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT_FIELDNAME,     // flat_value_state is valid
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT_BARE_FIELDNAME,
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT_VALUE, // flat_value_state is valid
  JSON_CALLBACK_PARSER_STATE_IN_OBJECT_VALUE_SPACE,
} JSON_CallbackParserState;

#define state_is_interim(state) ((state) <= JSON_CALLBACK_PARSER_STATE_INTERIM_EXPECTING_EOL)

// The flat-value-state is used to share the many substates possible for literal values.
// The following states use this substate:
//    JSON_CALLBACK_PARSER_STATE_INTERIM_IN_ARRAY_VALUE
//    JSON_CALLBACK_PARSER_STATE_IN_OBJECT_VALUE_SPACE
typedef enum
{
  // literal strings - parse states
  FLAT_VALUE_STATE_STRING,
#define FLAT_VALUE_STATE__STRING_START FLAT_VALUE_STATE_STRING
  FLAT_VALUE_STATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U,
  FLAT_VALUE_STATE_IN_UTF8_CHAR,        // parser->utf8_state gives details
  FLAT_VALUE_STATE_GOT_BACKSLASH_0,
  FLAT_VALUE_STATE_IN_BACKSLASH_UTF8,
  FLAT_VALUE_STATE_IN_BACKSLASH_CR,
  FLAT_VALUE_STATE_IN_BACKSLASH_X,
  FLAT_VALUE_STATE_IN_BACKSLASH_U,
#define FLAT_VALUE_STATE__STRING_END FLAT_VALUE_STATE_IN_BACKSLASH_U

  // literal numbers - parse states
  FLAT_VALUE_STATE_GOT_SIGN,
#define FLAT_VALUE_STATE__NUMBER_START FLAT_VALUE_STATE_GOT_SIGN
  FLAT_VALUE_STATE_GOT_0,
  FLAT_VALUE_STATE_IN_DIGITS,
  FLAT_VALUE_STATE_IN_OCTAL,
  FLAT_VALUE_STATE_IN_HEX_EMPTY,
  FLAT_VALUE_STATE_IN_HEX,
  FLAT_VALUE_STATE_GOT_E,
  FLAT_VALUE_STATE_GOT_E_PM,            // must get at least 1 digit
  FLAT_VALUE_STATE_GOT_E_DIGITS,
  FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT,
  FLAT_VALUE_STATE_GOT_DECIMAL_POINT,
#define FLAT_VALUE_STATE__NUMBER_END FLAT_VALUE_STATE_GOT_DECIMAL_POINT

  // literal barewords - true, false, null
  FLAT_VALUE_STATE_IN_NULL,
  FLAT_VALUE_STATE_IN_TRUE,
  FLAT_VALUE_STATE_IN_FALSE,

} FlatValueState;

static inline bool
flat_value_state_is_string(FlatValueState state)
{
  return FLAT_VALUE_STATE__STRING_START  <= state
      && state <= FLAT_VALUE_STATE__STRING_END;
}
static inline bool
flat_value_state_is_number(FlatValueState state)
{
  return FLAT_VALUE_STATE__NUMBER_START  <= state
      && state <= FLAT_VALUE_STATE__NUMBER_END;
}


typedef enum
{
  WHITESPACE_STATE_DEFAULT,
  WHITESPACE_STATE_SLASH,
  WHITESPACE_STATE_EOL_COMMENT,
  WHITESPACE_STATE_MULTILINE_COMMENT,
  WHITESPACE_STATE_MULTILINE_COMMENT_STAR,
  WHITESPACE_STATE_UTF8_E1,
  WHITESPACE_STATE_UTF8_E2,
  WHITESPACE_STATE_UTF8_E2_80,
  WHITESPACE_STATE_UTF8_E2_81,
  WHITESPACE_STATE_UTF8_E3,
  WHITESPACE_STATE_UTF8_E3_80,
} WhitespaceState;


typedef enum
{
  SCAN_END,
  SCAN_IN_VALUE,
  SCAN_ERROR
} ScanResult;

typedef enum
{
  UTF8_STATE_2_1,
  UTF8_STATE_3_1_got_zero,
  UTF8_STATE_3_1_got_nonzero,
  UTF8_STATE_3_2,
  UTF8_STATE_4_1_got_zero,
  UTF8_STATE_4_1_got_nonzero,
  UTF8_STATE_4_2,
  UTF8_STATE_4_3,
} UTF8State;


typedef ScanResult (*WhitespaceScannerFunc) (JSON_CallbackParser *parser,
                                             const uint8_t **p_at,
                                             const uint8_t  *end);


#define FLAT_VALUE_STATE_MASK              0x3f
#define FLAT_VALUE_STATE_MASKED(st)        ((st) & FLAT_VALUE_STATE_MASK)
#define FLAT_VALUE_STATE_SINGLE_QUOTED     0x40

#define FLAT_VALUE_STATE_REPLACE_ENUM_BITS(in, enum_value) \
  do{                                                      \
    in &= ~FLAT_VALUE_STATE_MASK;                          \
    in |= (enum_value);                                    \
  }while(0)

typedef struct JSON_CallbackParser_StackNode {
  uint8_t is_object : 1;                        // otherwise, it's an array
} JSON_CallbackParser_StackNode;

struct JSON_CallbackParser {
  JSON_CallbackParser_Options options;

  JSON_Callbacks callbacks;
  void *callback_data;

  uint64_t line_no, byte_no;            // 1-based, by tradition

  // NOTE: The stack doesn't include the outside array for ARRAY_OF_OBJECTS
  unsigned stack_depth;
  JSON_CallbackParser_StackNode *stack_nodes;

  JSON_CallbackParserState state;
  JSON_CallbackParserError error_code;

  // this is for strings and numbers; we also use it for quoted object field names.
  FlatValueState flat_value_state;

  WhitespaceScannerFunc whitespace_scanner;
  WhitespaceState whitespace_state;

  // Meaning of these two members depends on flat_value_state:
  //    * backslash sequence or UTF8 partial character
  unsigned flat_len;
  char flat_data[8];

  UTF8State utf8_state;

  size_t buffer_alloced;
  size_t buffer_length;
  char *buffer;
};

static inline void
buffer_set    (JSON_CallbackParser *parser,
               size_t               len,
               const uint8_t       *data)
{
  if (len > parser->buffer_alloced)
    {
      parser->buffer_alloced *= 2;
      while (len > parser->buffer_alloced)
        parser->buffer_alloced *= 2;
      free (parser->buffer);
      parser->buffer = malloc (parser->buffer_alloced);
    }
  memcpy (parser->buffer, data, len);
  parser->buffer_length = len;
}

static inline void
buffer_append (JSON_CallbackParser *parser,
               size_t               len,
               const uint8_t       *data)
{
  if (parser->buffer_length + len > parser->buffer_alloced)
    {
      parser->buffer_alloced *= 2;
      while (parser->buffer_length + len > parser->buffer_alloced)
        parser->buffer_alloced *= 2;
      parser->buffer = realloc (parser->buffer, parser->buffer_alloced);
    }
  memcpy (parser->buffer + parser->buffer_length, data, len);
  parser->buffer_length += len;
}

static inline void
buffer_append_c (JSON_CallbackParser *parser, uint8_t c)
{
  if (parser->buffer_length + 1 > parser->buffer_alloced)
    {
      parser->buffer_alloced *= 2;
      parser->buffer = realloc (parser->buffer, parser->buffer_alloced);
    }
  parser->buffer[parser->buffer_length++] = (char) c;
}

static inline const char *
buffer_nul_terminate (JSON_CallbackParser *parser)
{
  if (parser->buffer_length == parser->buffer_alloced)
    {
      parser->buffer_alloced *= 2;
      parser->buffer = realloc (parser->buffer, parser->buffer_alloced);
    }
  parser->buffer[parser->buffer_length] = 0;
  return parser->buffer;
}

static const char *
error_code_to_string (JSON_CallbackParserError code)
{
  switch (code)
    {
    case JSON_CALLBACK_PARSER_ERROR_NONE:
      return "No error";                        // should not happen

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_MEMBER_NAME:
      return "Expected member name";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_COLON:
      return "Expected colon (':')";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_VALUE:
      return "Expected value (a number, string, object, array or constant)";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_COMMA_OR_RBRACKET:
      return "Expected ',' or ']' in array";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_COMMA_OR_RBRACE:
      return "Expected ',' or ']' in object";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_HEX_DIGIT:
      return "Expected hexidecimal digit (0-9, a-f, A-F)";

    case JSON_CALLBACK_PARSER_ERROR_EXPECTED_STRUCTURED_VALUE:
      return "Expected object or array (aka a Structured Value) at toplevel";

    case JSON_CALLBACK_PARSER_ERROR_EXTRA_COMMA:
      return "Got multiple commas (','): not allowed";

    case JSON_CALLBACK_PARSER_ERROR_TRAILING_COMMA:
      return "Got trailing comma (',') at end of object or array";

    case JSON_CALLBACK_PARSER_ERROR_SINGLE_QUOTED_STRING_NOT_ALLOWED:
      return "Single-quoted strings are not allowed";

    case JSON_CALLBACK_PARSER_ERROR_STACK_DEPTH_EXCEEDED:
      return "JSON nested too deeply: stack-depth exceeded";

    case JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR:
      return "Unexpected character";

    case JSON_CALLBACK_PARSER_ERROR_BAD_BAREWORD:
      return "Unknown bareword value";

    case JSON_CALLBACK_PARSER_ERROR_PARTIAL_RECORD:
      return "Partial record (end-of-file in middle of value)";

    case JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG:
      return "Invalid UTF-8 data (overlong)";

    case JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_INITIAL_BYTE:
    case JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE:
    case JSON_CALLBACK_PARSER_ERROR_UTF8_SURROGATE_PAIR_NOT_ALLOWED:
    case JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR:
    case JSON_CALLBACK_PARSER_ERROR_STRING_CONTROL_CHARACTER:
    case JSON_CALLBACK_PARSER_ERROR_QUOTED_NEWLINE:
    case JSON_CALLBACK_PARSER_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL:
    case JSON_CALLBACK_PARSER_ERROR_BACKSLASH_X_NOT_ALLOWED:

  // errors that can occur while validating a number
    case JSON_CALLBACK_PARSER_ERROR_HEX_NOT_ALLOWED:
    case JSON_CALLBACK_PARSER_ERROR_OCTAL_NOT_ALLOWED:
    case JSON_CALLBACK_PARSER_ERROR_NON_OCTAL_DIGIT:
    case JSON_CALLBACK_PARSER_ERROR_LEADING_DECIMAL_POINT_NOT_ALLOWED:
    case JSON_CALLBACK_PARSER_ERROR_TRAILING_DECIMAL_POINT_NOT_ALLOWED:
    case JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER:

    case JSON_CALLBACK_PARSER_ERROR_INTERNAL:

    }
  return "Invalid JSON Parser Error Code";
}

// invoking callbacks
static inline bool
do_callback_start_object (JSON_CallbackParser *parser)
{
  return parser->callbacks.start_object (parser->callback_data);
}
static inline bool
do_callback_end_object (JSON_CallbackParser *parser)
{
  return parser->callbacks.end_object (parser->callback_data);
}

static inline bool
do_callback_start_array (JSON_CallbackParser *parser)
{
  return parser->callbacks.start_array (parser->callback_data);
}

static inline bool
do_callback_end_array (JSON_CallbackParser *parser)
{
  return parser->callbacks.end_array (parser->callback_data);
}

static inline bool
do_callback_object_key  (JSON_CallbackParser *parser)
{
  return parser->callbacks.object_key (parser->buffer_length,
                                       buffer_nul_terminate (parser),
                                       parser->callback_data);
}
static inline bool
do_callback_string      (JSON_CallbackParser *parser)
{
  return parser->callbacks.string_value (parser->buffer_length,
                                         buffer_nul_terminate (parser),
                                         parser->callback_data);
}
static inline bool
do_callback_number      (JSON_CallbackParser *parser)
{
  return parser->callbacks.number_value (parser->buffer_length,
                                         buffer_nul_terminate (parser),
                                         parser->callback_data);
}
static inline bool
do_callback_boolean     (JSON_CallbackParser *parser, bool v)
{
  return parser->callbacks.boolean_value (v, parser->callback_data);
}
static inline bool
do_callback_null        (JSON_CallbackParser *parser)
{
  return parser->callbacks.null_value (parser->callback_data);
}
static inline bool
do_callback_error       (JSON_CallbackParser *parser)
{
  JSON_CallbackParser_ErrorInfo error_info;
  error_info.code = parser->error_code;
  error_info.line_no = parser->line_no;
  error_info.byte_no = parser->byte_no;
  error_info.message = error_code_to_string (parser->error_code);
  switch (parser->error_code)
    {
    case JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR:
      error_info.message2 = byte_names[byte_name_index[parser->error_char]];
      break;
    default:
      error_info.message2 = NULL;
      break;
    }
  const char *message;
  const char *message2;         /// may be NULL
  return parser->callbacks.error (&error_info, parser->callback_data);
}

static ScanResult
scan_whitespace_json   (JSON_CallbackParser *parser,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
  const uint8_t *at = *p_at;
  (void) parser;
  while (at < end && IS_SPACE (*at))
    at++;
  *p_at = at;
  return SCAN_END;
}
static ScanResult
scan_whitespace_json5  (JSON_CallbackParser *parser,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
#define CASE(shortname) case_##shortname: case WHITESPACE_STATE_##shortname
#define GOTO_WHITESPACE_STATE(shortname)                      \
do {                                                          \
  parser->whitespace_state = WHITESPACE_STATE_##shortname; \
  if (at == end) {                                            \
    *p_at = at;                                               \
    return SCAN_IN_VALUE;                                     \
  }                                                           \
  goto case_##shortname;                                      \
} while(0)
  const uint8_t *at = *p_at;
  (void) parser;
  switch (parser->whitespace_state)
    {
    CASE(DEFAULT):
      while (at < end && IS_SPACE(*at))
        at++;
      if (*at == '/')
        {
          at++;
          GOTO_WHITESPACE_STATE(SLASH);
        }
      else if (*at == 0xe1)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E1);
        }
      else if (*at == 0xe2)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E2);
        }
      else if (*at == 0xe3)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E3);
        }
      else
        goto nonws_nonascii;
    CASE(SLASH):
      if (*at == '*')
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT);
        }
      else if (*at == '/')
        {
          at++;
          GOTO_WHITESPACE_STATE(EOL_COMMENT);
        }
      else
        {
          *p_at = at;
          parser->error_char = *at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR;
          return SCAN_ERROR;
        }

    CASE(EOL_COMMENT):
      while (at < end && *at != '\n')
        at++;
      if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      at++;
      GOTO_WHITESPACE_STATE(DEFAULT);

    CASE(MULTILINE_COMMENT):
      while (at < end)
        {
          if (*at == '*')
            {
              at++;
              GOTO_WHITESPACE_STATE(MULTILINE_COMMENT_STAR);
            }
          else
            at++;
        }
      *p_at = at;
      return SCAN_IN_VALUE;

    CASE(MULTILINE_COMMENT_STAR):
      if (*at == '/')
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else if (*at == '*')
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT_STAR);
        }
      else
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT);
        }

    CASE(UTF8_E1):
      if (*at == 0x9a)
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E2):
      if (*at == 0x80)
        GOTO_WHITESPACE_STATE(UTF8_E2_80);
      else if (*at == 0x81)
        GOTO_WHITESPACE_STATE(UTF8_E2_81);
      else
        goto nonws_nonascii;
    CASE(UTF8_E2_80):
      if (*at == 0x80 /* u+2000 en quad */
       || *at == 0x81 /* u+2001 em quad */
       || *at == 0x84 /* u+2004 three-per-em space */
       || *at == 0x85 /* u+2005 FOUR-PER-EM SPACE */
       || *at == 0x87 /* u+2007 FIGURE SPACE  */
       || *at == 0x88 /* u+2008 PUNCTUATION SPACE */
       || *at == 0x89 /* u+2009 THIN SPACE  */
       || *at == 0x8a /* u+200A HAIR SPACE  */
       || *at == 0x8b)/* u+200B ZERO WIDTH SPACE  */
      {
        at++;
        GOTO_WHITESPACE_STATE(DEFAULT);
      }
    CASE(UTF8_E2_81):
      if (*at == 0x9f) /* U+205F MEDIUM MATHEMATICAL SPACE  */
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E3):
      if (*at == 0x80)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E3_80);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E3_80):
      if (*at == 0x80)
        {
          /* U+3000 IDEOGRAPHIC SPACE */
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;
    }
#undef CASE
#undef GOTO_WHITESPACE_STATE


nonws_nonascii:
  parser->error_char = *at;
  parser->error_code = JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR;
  return SCAN_ERROR;
}
static ScanResult
scan_whitespace_generic(JSON_CallbackParser *parser,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
#define CASE(shortname) case_##shortname: case WHITESPACE_STATE_##shortname
#define GOTO_WHITESPACE_STATE(shortname)                      \
do {                                                          \
  parser->whitespace_state = WHITESPACE_STATE_##shortname; \
  if (at == end) {                                            \
    *p_at = at;                                               \
    return SCAN_IN_VALUE;                                     \
  }                                                           \
  goto case_##shortname;                                      \
} while(0)
  const uint8_t *at = *p_at;
  (void) parser;
  switch (parser->whitespace_state)
    {
    CASE(DEFAULT):
      while (at < end && IS_SPACE(*at))
        at++;
      if (*at == '/' && (parser->options.ignore_single_line_comments
                       || parser->options.ignore_multi_line_comments))
        {
          at++;
          GOTO_WHITESPACE_STATE(SLASH);
        }
      else if (*at == 0xe1 && parser->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E1);
        }
      else if (*at == 0xe2 && parser->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E2);
        }
      else if (*at == 0xe3 && parser->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E3);
        }
      else
        goto nonws_nonascii;
    CASE(SLASH):
      if (*at == '*' && parser->options.ignore_multi_line_comments)
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT);
        }
      else if (*at == '/' && parser->options.ignore_single_line_comments)
        {
          at++;
          GOTO_WHITESPACE_STATE(EOL_COMMENT);
        }
      else
        {
          *p_at = at;
          parser->error_char = *at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR;
          return SCAN_ERROR;
        }

    CASE(EOL_COMMENT):
      while (at < end && *at != '\n')
        at++;
      if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      at++;
      GOTO_WHITESPACE_STATE(DEFAULT);

    CASE(MULTILINE_COMMENT):
      while (at < end)
        {
          if (*at == '*')
            {
              at++;
              GOTO_WHITESPACE_STATE(MULTILINE_COMMENT_STAR);
            }
          else
            at++;
        }
      *p_at = at;
      return SCAN_IN_VALUE;

    CASE(MULTILINE_COMMENT_STAR):
      if (*at == '/')
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else if (*at == '*')
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT_STAR);
        }
      else
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT);
        }

    CASE(UTF8_E1):
      if (*at == 0x9a)
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E2):
      if (*at == 0x80)
        GOTO_WHITESPACE_STATE(UTF8_E2_80);
      else if (*at == 0x81)
        GOTO_WHITESPACE_STATE(UTF8_E2_81);
      else
        goto nonws_nonascii;
    CASE(UTF8_E2_80):
      if (*at == 0x80 /* u+2000 en quad */
       || *at == 0x81 /* u+2001 em quad */
       || *at == 0x84 /* u+2004 three-per-em space */
       || *at == 0x85 /* u+2005 FOUR-PER-EM SPACE */
       || *at == 0x87 /* u+2007 FIGURE SPACE  */
       || *at == 0x88 /* u+2008 PUNCTUATION SPACE */
       || *at == 0x89 /* u+2009 THIN SPACE  */
       || *at == 0x8a /* u+200A HAIR SPACE  */
       || *at == 0x8b)/* u+200B ZERO WIDTH SPACE  */
      {
        at++;
        GOTO_WHITESPACE_STATE(DEFAULT);
      }
    CASE(UTF8_E2_81):
      if (*at == 0x9f) /* U+205F MEDIUM MATHEMATICAL SPACE  */
        {
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E3):
      if (*at == 0x80)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E3_80);
        }
      else
        goto nonws_nonascii;

    CASE(UTF8_E3_80):
      if (*at == 0x80)
        {
          /* U+3000 IDEOGRAPHIC SPACE */
          at++;
          GOTO_WHITESPACE_STATE(DEFAULT);
        }
      else
        goto nonws_nonascii;
    }
#undef CASE
#undef GOTO_WHITESPACE_STATE


nonws_nonascii:
  parser->error_char = *at;
  parser->error_code = JSON_CALLBACK_PARSER_ERROR_UNEXPECTED_CHAR;
  return SCAN_ERROR;
}


size_t
json_callback_parser_options_get_memory_size (const JSON_CallbackParser_Options *options)
{
  return sizeof (JSON_CallbackParser)
       + options->max_stack_depth * sizeof (JSON_CallbackParser_StackNode);
}

// note that JSON_CallbackParser allocates no further memory, so this can 
// be embedded somewhere.
JSON_CallbackParser *
json_callback_parser_new (const JSON_Callbacks *callbacks,
                          void *callback_data,
                          const JSON_CallbackParser_Options *options)
{
  JSON_CallbackParser *parser = malloc (sizeof (JSON_CallbackParser));
  parser->options = *options;
  parser->stack_depth = 0;
  parser->stack_nodes = (JSON_CallbackParser_StackNode *) (parser + 1);
  parser->state = JSON_CALLBACK_PARSER_STATE_INTERIM;
  parser->error_code = JSON_CALLBACK_PARSER_ERROR_NONE;
  parser->callbacks = *callbacks;
  parser->callback_data = callback_data;

  // select optimized whitespace scanner
  if (!options->ignore_single_line_comments
   && !options->ignore_multi_line_comments
   && !options->disallow_extra_whitespace
   && !options->ignore_unicode_whitespace)
    parser->whitespace_scanner = scan_whitespace_json;
  else
  if ( options->ignore_single_line_comments
   &&  options->ignore_multi_line_comments
   && !options->disallow_extra_whitespace
   &&  options->ignore_unicode_whitespace)
    parser->whitespace_scanner = scan_whitespace_json5;
  else
    parser->whitespace_scanner = scan_whitespace_generic;
    
  return parser;
}

static inline ScanResult
utf8_validate_char (JSON_CallbackParser *parser,
                    const uint8_t **p_at,
                    const uint8_t *end)
{
  const uint8_t *at = *p_at;
  if ((*at & 0xe0) == 0xc0)
    {
      if ((*at & 0x1f) < 2)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // 2 byte sequence
      if (at + 1 == end)
        {
          parser->utf8_state = UTF8_STATE_2_1;
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 2;
      return SCAN_END;
    }
  else if ((*at & 0xf0) == 0xe0)
    {
      // 3 byte sequence
      if (at + 1 == end)
        {
          *p_at = at + 1;
          parser->utf8_state = ((at[0] & 0x0f) != 0)
                                ? UTF8_STATE_3_1_got_nonzero
                                : UTF8_STATE_3_1_got_zero;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if ((at[0] & 0x0f) == 0 && (at[1] & 0x40) == 0)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      if (at + 2 == end)
        {
          parser->utf8_state = UTF8_STATE_3_2;
          return SCAN_IN_VALUE;
        }
      if ((at[2] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 3;
      return SCAN_END;
    }
  else if ((*at & 0xf8) == 0xf0)
    {
      // 4 byte sequence
      if (at + 1 == end)
        {
          parser->utf8_state = ((at[0] & 0x07) != 0)
                                ? UTF8_STATE_4_1_got_nonzero
                                : UTF8_STATE_4_1_got_zero;
          *p_at = at + 1;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if ((at[0] & 0x07) == 0 && (at[1] & 0x30) == 0)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      if (at + 2 == end)
        {
          parser->utf8_state = UTF8_STATE_4_2;
        }
      if ((at[2] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if (at + 3 == end)
        {
          parser->utf8_state = UTF8_STATE_4_3;
        }
      if ((at[3] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 4;
      return SCAN_END;
    }
  else
    {
      // invalid initial utf8-byte
      parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_INITIAL_BYTE;
      return SCAN_ERROR;
    }
}
static inline ScanResult
utf8_continue_validate_char (JSON_CallbackParser *parser,
                             const uint8_t **p_at,
                             const uint8_t *end)
{
  const uint8_t *at = *p_at;
  switch (parser->utf8_state)
    {
    case UTF8_STATE_2_1:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 1;
      return SCAN_END;

    case UTF8_STATE_3_1_got_zero:
      if ((at[0] & 0x40) == 0)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // fall through

    case UTF8_STATE_3_1_got_nonzero:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if (at + 1 == end)
        {
          parser->utf8_state = UTF8_STATE_3_2;
          *p_at = at + 1;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          parser->utf8_state = UTF8_STATE_3_1_got_nonzero;
          return SCAN_ERROR;
        }
      *p_at = at + 2;
      return SCAN_END;

    case UTF8_STATE_3_2:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          parser->utf8_state = UTF8_STATE_3_2;
          return SCAN_ERROR;
        }
      *p_at = at + 1;
      return SCAN_END;

    case UTF8_STATE_4_1_got_zero:
      if ((at[0] & 0x30) == 0)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // fall through
    case UTF8_STATE_4_1_got_nonzero:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          parser->utf8_state = UTF8_STATE_4_1_got_nonzero;
          return SCAN_ERROR;
        }
      at++;
      if (at == end)
        {
          *p_at = at;
          parser->utf8_state = UTF8_STATE_4_2;
          return SCAN_IN_VALUE;
        }
      // fall-through

    case UTF8_STATE_4_2:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          parser->utf8_state = UTF8_STATE_4_2;
          return SCAN_ERROR;
        }
      at++;
      if (at == end)
        {
          *p_at = at;
          parser->utf8_state = UTF8_STATE_4_3;
          return SCAN_IN_VALUE;
        }
      // fall-through

    case UTF8_STATE_4_3:
      if ((at[0] & 0xc0) != 0x80)
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_TRAILING_BYTE;
          parser->utf8_state = UTF8_STATE_4_3;
          return SCAN_ERROR;
        }
      *p_at = at + 1;
      return SCAN_END;
    }
}

static inline int
is_number_end_char (char c)
{
  return IS_SPACE(c) || c == ',' || c == '}' || c == ']';
}

static inline int
is_flat_value_char (JSON_CallbackParser *parser,
                    char            c)
{
  if (c == '"') return 1;
  if ('0' <= c && c <= '9') return 1;
  if (c == '\'' && parser->options.permit_single_quote_strings) return 1;
  if (c == '.' && parser->options.permit_leading_decimal_point) return 1;
  return 0;
}

static inline FlatValueState
initial_flat_value_state (char            c)
{
  if (c == '"') return FLAT_VALUE_STATE_STRING;
  if ('1' <= c && c <= '9') return FLAT_VALUE_STATE_IN_DIGITS;
  if (c == '0') return FLAT_VALUE_STATE_GOT_0;
  if (c == '+' || c == '-') return FLAT_VALUE_STATE_GOT_SIGN;
  if (c == '\'') return FLAT_VALUE_STATE_STRING | FLAT_VALUE_STATE_SINGLE_QUOTED;
  if (c == '.') return FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT;
  assert(0);
  return 0;
}

static inline char
flat_value_state_to_end_quote_char (FlatValueState state)
{
  return (state & FLAT_VALUE_STATE_SINGLE_QUOTED) ? '\'' : '"';
}

static ScanResult
generic_bareword_handler (JSON_CallbackParser *parser,
                          const uint8_t**p_at,
                          const uint8_t *end,
                          const char    *bareword,
                          size_t         bareword_len)
{
  const uint8_t *at = *p_at;
  while (at < end
      && parser->flat_len < bareword_len
      && (char)(*at) == bareword[parser->flat_len])
    {
      parser->flat_len += 1;
      at++;
    }
  if (at == end)
    {
      *p_at = at;
      return SCAN_IN_VALUE;
    }
  if (is_number_end_char(*at))
    {
      *p_at = at;
      return SCAN_END;
    }
  parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_BAREWORD;
  *p_at = at;
  return SCAN_ERROR;
}

static ScanResult
scan_flat_value (JSON_CallbackParser *parser,
                 const uint8_t **p_at,
                 const uint8_t  *end)
{
  const uint8_t *at = *p_at;
  char end_quote_char;

#define FLAT_VALUE_GOTO_STATE(st_shortname) \
  do{ \
    FLAT_VALUE_STATE_REPLACE_ENUM_BITS (parser->flat_value_state, FLAT_VALUE_STATE_##st_shortname); \
    if (at == end) \
      return SCAN_IN_VALUE; \
    goto case_##st_shortname; \
  }while(0)

  switch (FLAT_VALUE_STATE_MASKED (parser->flat_value_state))
    {
    case_STRING:
    case FLAT_VALUE_STATE_STRING:
      end_quote_char = flat_value_state_to_end_quote_char (parser->flat_value_state);
      while (at < end)
        {
          if (*at == '\\')
            {
              at++;
              parser->flat_len = 0;
              FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_SEQUENCE);
            }
          else if (*at == end_quote_char)
            {
              *p_at = at + 1;
              return SCAN_END;
            }
          else if ((*at & 0x80) == 0)
            {
              // ascii range, but are naked control sequences allowed?
              buffer_append_c (parser, *at);
              at++;
              continue;
            }
          else
            {
              const uint8_t *start = at;
              switch (utf8_validate_char (parser, &at, end))
                {
                case SCAN_END:
                  buffer_append (parser, at - start, start);
                  break;
                case SCAN_IN_VALUE:
                  // note that parser->utf8_state is set
                  // when we get this return-value.
                  assert(at == end);
                  buffer_append (parser, at-start, start);
                  FLAT_VALUE_STATE_REPLACE_ENUM_BITS(parser->flat_value_state,
                                                     FLAT_VALUE_STATE_IN_UTF8_CHAR);
                  *p_at = at;
                  return SCAN_IN_VALUE;
                case SCAN_ERROR:
                  return SCAN_ERROR;
                }
            }
        }
      *p_at = at;
      return SCAN_IN_VALUE;

    case_IN_BACKSLASH_SEQUENCE:
    case FLAT_VALUE_STATE_IN_BACKSLASH_SEQUENCE:
      switch (*at)
        {
        // one character ecape codes handled here.
        case 'r':
          buffer_append_c (parser, '\r');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case 't':
          buffer_append_c (parser, '\t');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case 'n':
          buffer_append_c (parser, '\n');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case 'b':
          buffer_append_c (parser, '\b');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case 'v':
          buffer_append_c (parser, '\v');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case 'f':
          buffer_append_c (parser, '\f');
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);
        case '"': case '\\': case '\'':
          buffer_append_c (parser, *at);
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);

        case '0':      
          if (parser->options.permit_backslash_0)
            {
              buffer_append_c (parser, 0);
              at++;
              FLAT_VALUE_GOTO_STATE(GOT_BACKSLASH_0);
            }
          else
            {
              // literal "0" digit
              buffer_append_c (parser, '0');
              at++;
              FLAT_VALUE_GOTO_STATE(STRING);
            }

        case '\n':
          if (!parser->options.permit_line_continuations_in_strings)
            {
              parser->error_code = JSON_CALLBACK_PARSER_ERROR_QUOTED_NEWLINE;
              return SCAN_ERROR;
            }
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);

        case '\r':
          // must also accept CRLF = 13,10
          if (!parser->options.permit_line_continuations_in_strings)
            {
              parser->error_code = JSON_CALLBACK_PARSER_ERROR_QUOTED_NEWLINE;
              return SCAN_ERROR;
            }
          at++;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_CR);

        case 'x': case 'X':
          if (!parser->options.permit_backslash_x)
            {
              parser->error_code = JSON_CALLBACK_PARSER_ERROR_BACKSLASH_X_NOT_ALLOWED;
              return SCAN_ERROR;
            }
          at++;
          parser->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_X);

        case 'u': case 'U':
          at++;
          parser->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_U);

        default:
          if (*at < 0x80)
            {
              // single-byte pass-through
              at++;
              FLAT_VALUE_GOTO_STATE(STRING);
            }
          else
            {
              if (*at >= 0xf8)
                {
                  parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF8_BAD_INITIAL_BYTE;
                  *p_at = at;
                  return SCAN_ERROR;
                }
              else
                {
                  // multi-byte passthrough character
                  parser->flat_data[0] = *at;
                  parser->flat_len = 1;
                  switch (utf8_validate_char (parser, &at, end))
                    {
                    case SCAN_END:
                      FLAT_VALUE_GOTO_STATE(STRING);

                    case SCAN_IN_VALUE:
                      *p_at = at;
                      parser->flat_value_state = FLAT_VALUE_STATE_IN_BACKSLASH_UTF8;
                      return SCAN_IN_VALUE;

                    case SCAN_ERROR:
                      *p_at = at;
                      return SCAN_ERROR;
                    }
                }
            }
        }

    case_IN_BACKSLASH_CR:
    case FLAT_VALUE_STATE_IN_BACKSLASH_CR:
      if (*at == '\n')
        at++;
      FLAT_VALUE_GOTO_STATE(STRING);

    case_IN_BACKSLASH_X:
    case FLAT_VALUE_STATE_IN_BACKSLASH_X:
      while (parser->flat_len < 2 && at < end && IS_HEX_DIGIT(*at))
        {
          parser->flat_len++;
          at++;
        }
      if (parser->flat_len == 2)
        FLAT_VALUE_GOTO_STATE(STRING);
      else if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      else
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_EXPECTED_HEX_DIGIT;
          return SCAN_ERROR;
        }

    case_IN_BACKSLASH_U:
    case FLAT_VALUE_STATE_IN_BACKSLASH_U:
      while (parser->flat_len < 4 && at < end && IS_HEX_DIGIT(*at))
        {
          parser->flat_data[parser->flat_len] = *at;
          parser->flat_len++;
          at++;
        }
      if (parser->flat_len == 4)
        {
          parser->flat_data[4] = 0;
          uint32_t code = strtoul (parser->flat_data, NULL, 16);
          if (IS_HI_SURROGATE(code))
            {
              FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE);
            }
          else if (IS_LO_SURROGATE(code))
            {
              parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR;
              return SCAN_ERROR;
            }
          else
            {
              // any other validation to do?
              FLAT_VALUE_GOTO_STATE(STRING);
            }
        }
      else if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      else
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_EXPECTED_HEX_DIGIT;
          return SCAN_ERROR;
        }
    case_GOT_HI_SURROGATE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE:
      if (*at == '\\')
        FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE);
      else
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR;
          return SCAN_ERROR;
        }
    
    case_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
      if (*at == 'u' || *at == 'U')
        {
          parser->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U);
        }
      else
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR;
          return SCAN_ERROR;
        }

    case_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U:
      while (parser->flat_len < 4 && at < end && IS_HEX_DIGIT(*at))
        {
          parser->flat_data[parser->flat_len] = *at;
          parser->flat_len++;
          at++;
        }
      if (parser->flat_len == 4)
        {
          parser->flat_data[4] = 0;
          uint32_t code = strtoul (parser->flat_data, NULL, 16);
          if (IS_LO_SURROGATE(code))
            FLAT_VALUE_GOTO_STATE(STRING);
          else
            {
              *p_at = at;
              parser->error_code = JSON_CALLBACK_PARSER_ERROR_UTF16_BAD_SURROGATE_PAIR;
              return SCAN_ERROR;
            }
        }
      else if (at < end)
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_EXPECTED_HEX_DIGIT;
          return SCAN_ERROR;
        }
      else
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }

    case_GOT_BACKSLASH_0:
    case FLAT_VALUE_STATE_GOT_BACKSLASH_0:
      if (IS_DIGIT(*at))
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL;
          return SCAN_ERROR;
        }
      FLAT_VALUE_GOTO_STATE(STRING);

    //case_IN_BACKSLASH_UTF8:
    case FLAT_VALUE_STATE_IN_BACKSLASH_UTF8:
      switch (utf8_continue_validate_char (parser, &at, end))
        {
        case SCAN_END:
          FLAT_VALUE_GOTO_STATE(STRING);

        case SCAN_IN_VALUE:
          *p_at = at;
          return SCAN_IN_VALUE;
        case SCAN_ERROR:
          *p_at = at;
          return SCAN_ERROR;
        }

    //case_IN_UTF8_CHAR:
    case FLAT_VALUE_STATE_IN_UTF8_CHAR:
      switch (utf8_continue_validate_char (parser, &at, end))
        {
        case SCAN_END:
          FLAT_VALUE_GOTO_STATE(STRING);

        case SCAN_IN_VALUE:
          *p_at = at;
          return SCAN_IN_VALUE;

        case SCAN_ERROR:
          *p_at = at;
          return SCAN_ERROR;
        }

    //case_GOT_SIGN:
    case FLAT_VALUE_STATE_GOT_SIGN:
      if (*at == '0')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_0);
        }
      else if ('1' <= *at && *at <= '9')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(IN_DIGITS);
        }
      else if (*at == '.' && parser->options.permit_leading_decimal_point)
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_LEADING_DECIMAL_POINT);
        }
      else
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          *p_at = at;
          return SCAN_ERROR;
        }

    case_GOT_0:
    case FLAT_VALUE_STATE_GOT_0:
      if ((*at == 'x' || *at == 'X') && parser->options.permit_hex_numbers)
        {
          at++;
          FLAT_VALUE_GOTO_STATE(IN_HEX_EMPTY);
        }
      else if (('0' <= *at && *at <= '7') && parser->options.permit_octal_numbers)
        {
          at++;
          FLAT_VALUE_GOTO_STATE(IN_OCTAL);
        }
      else if (*at == '.')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_DECIMAL_POINT);
        }
      else if (*at == 'e' || *at == 'E')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_E);
        }
      else if (IS_ALNUM(*at) && *at == '_')
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          *p_at = at;
          return SCAN_ERROR;
        }
      else
        {
          // simply 0
          *p_at = at;
          return SCAN_END;
        }
    
    case_IN_DIGITS:
    case FLAT_VALUE_STATE_IN_DIGITS:
        while ((at < end) && ('0' <= *at && *at <= '9'))
          at++;
        if (at == end)
          {
            *p_at = at;
            return SCAN_IN_VALUE;
          }
        if (*at == 'e' || *at == 'E')
          {
            at++;
            FLAT_VALUE_GOTO_STATE(GOT_E);
          }
        else if (*at == '.')
          {
            at++;
            FLAT_VALUE_GOTO_STATE(GOT_DECIMAL_POINT);
          }
        else
          {
            *p_at = at;
            return SCAN_END;
          }

    case_IN_OCTAL:
    case FLAT_VALUE_STATE_IN_OCTAL:
      while (at < end && IS_OCT_DIGIT (*at))
        at++;
      if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      else if (is_number_end_char (*at))
        {
          *p_at = at;
          return SCAN_END;
        }
      else
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }

    case_IN_HEX_EMPTY:
    case FLAT_VALUE_STATE_IN_HEX_EMPTY:
      if (!IS_HEX_DIGIT (*at))
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          return SCAN_END;
        }
      at++;
      FLAT_VALUE_GOTO_STATE(IN_HEX);

    case_IN_HEX:
    case FLAT_VALUE_STATE_IN_HEX:
      while (at < end && IS_HEX_DIGIT(*at))
        at++;
      *p_at = at;
      return SCAN_END;

    case_GOT_E:
    case FLAT_VALUE_STATE_GOT_E:
      if (*at == '-' || *at == '+')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_E_DIGITS);
        }
      else
        FLAT_VALUE_GOTO_STATE(GOT_E_PM);

    case_GOT_E_PM:
    case FLAT_VALUE_STATE_GOT_E_PM:
      if (IS_DIGIT (*at))
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_E_DIGITS);
        }
      else
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          return SCAN_END;
        }

    case_GOT_E_DIGITS:
    case FLAT_VALUE_STATE_GOT_E_DIGITS:
      while (at < end && IS_DIGIT (*at))
        {
          at++;
        }
      if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      if (is_number_end_char (*at))
        {
          *p_at = at;
          return SCAN_END;
        }
      else
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }
      

    case_GOT_LEADING_DECIMAL_POINT:
    case FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT:
      if (!IS_DIGIT (*at))
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          *p_at = at;
          return SCAN_ERROR;
        }
      at++;
      FLAT_VALUE_GOTO_STATE(GOT_DECIMAL_POINT);

    case_GOT_DECIMAL_POINT:
    case FLAT_VALUE_STATE_GOT_DECIMAL_POINT:
      while (at < end && IS_DIGIT (*at))
        at++;
      if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      if (*at == 'e' || *at == 'E')
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_E);
        }
      if (!is_number_end_char(*at))
        {
          *p_at = at;
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }
      *p_at = at;
      return SCAN_END;

    //case_IN_NULL:
    case FLAT_VALUE_STATE_IN_NULL:
      *p_at = at;
      return generic_bareword_handler (parser, p_at, end, "null", 4);

    //case_IN_TRUE:
    case FLAT_VALUE_STATE_IN_TRUE:
      *p_at = at;
      return generic_bareword_handler (parser, p_at, end, "true", 4);

    //case_IN_FALSE:
    case FLAT_VALUE_STATE_IN_FALSE:
      *p_at = at;
      return generic_bareword_handler (parser, p_at, end, "false", 5);
    }

#undef FLAT_VALUE_GOTO_STATE
  assert(0);
  return SCAN_ERROR;
}

static int
flat_value_can_terminate (JSON_CallbackParser *parser)
{
  switch (parser->flat_value_state)
    {
    case FLAT_VALUE_STATE_STRING:
    case FLAT_VALUE_STATE_IN_BACKSLASH_SEQUENCE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U:
    case FLAT_VALUE_STATE_IN_UTF8_CHAR:
    case FLAT_VALUE_STATE_GOT_BACKSLASH_0:
    case FLAT_VALUE_STATE_IN_BACKSLASH_UTF8:
    case FLAT_VALUE_STATE_IN_BACKSLASH_CR:
    case FLAT_VALUE_STATE_IN_BACKSLASH_X:
    case FLAT_VALUE_STATE_IN_BACKSLASH_U:
      return 0;

    // literal numbers - parse states
    case FLAT_VALUE_STATE_GOT_0:
    case FLAT_VALUE_STATE_IN_DIGITS:
    case FLAT_VALUE_STATE_IN_OCTAL:
    case FLAT_VALUE_STATE_GOT_E_DIGITS:
    case FLAT_VALUE_STATE_IN_HEX:
    case FLAT_VALUE_STATE_GOT_DECIMAL_POINT:    // XXX
      return 1;
    case FLAT_VALUE_STATE_IN_HEX_EMPTY:
    case FLAT_VALUE_STATE_GOT_SIGN:
    case FLAT_VALUE_STATE_GOT_E:
    case FLAT_VALUE_STATE_GOT_E_PM:            // must get at least 1 digit
    case FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT:
      return 0;

  // literal barewords - true, false, null
    case FLAT_VALUE_STATE_IN_NULL:
    case FLAT_VALUE_STATE_IN_TRUE:
    case FLAT_VALUE_STATE_IN_FALSE:
      return 0;
    }
}

JSON_CALLBACK_PARSER_FUNC_DEF
bool
json_callback_parser_feed (JSON_CallbackParser *parser,
                           size_t          len,
                           const uint8_t  *data)
{
  const uint8_t *end = data + len;
  const uint8_t *at = data;

#define SKIP_CHAR_TYPE(predicate)                                    \
  do {                                                               \
    while (at < end  &&  predicate(*at))                             \
      at++;                                                          \
    if (at == end)                                                   \
      goto at_end;                                                   \
  }while(0)

#define SKIP_WS()   SKIP_CHAR_TYPE(IS_SPACE)

#define RETURN_ERROR_CODE()                                                \
  do{                                                                      \
    return JSON_CALLBACK_PARSER_RESULT_ERROR;                                    \
  }while(0)

#define RETURN_ERROR(error_code_shortname)                                 \
  do{                                                                      \
    parser->error_code = JSON_CALLBACK_PARSER_ERROR_ ## error_code_shortname; \
    return false;                                                     \
  }while(0)

#define GOTO_STATE(state_shortname)                                    \
  do{                                                                 \
    parser->state = JSON_CALLBACK_PARSER_STATE_ ## state_shortname;   \
    if (at == end)                                                    \
      goto at_end;                                                    \
    goto case_##state_shortname;                                      \
  }while(0)

#define PUSH(is_obj)                                                  \
  do{                                                                 \
    if (parser->stack_depth == parser->options.max_stack_depth)       \
      RETURN_ERROR(STACK_DEPTH_EXCEEDED);                             \
    JSON_CallbackParser_StackNode *n = parser->stack_nodes + parser->stack_depth;\
    n->is_object = (is_obj);                                          \
    ++parser->stack_depth;                                            \
  }while(0)
  
#define PUSH_OBJECT()                                                 \
  do{                                                                 \
    if (!do_callback_start_object(parser))                            \
      return false;                                                   \
    PUSH(1);                                                          \
  }while(0)
#define PUSH_ARRAY()                                                  \
  do{                                                                 \
    if (!do_callback_start_array(parser))                             \
      return false;                                                   \
    PUSH(0);                                                          \
  }while(0)

#define POP()                                                         \
  do{                                                                 \
    assert(parser->stack_depth > 0);                                  \
    if (parser->stack_nodes[parser->stack_depth - 1].is_object)       \
      do_callback_end_object(parser);                                 \
    else                                                              \
      do_callback_end_array(parser);                                  \
    --parser->stack_depth;                                            \
    if (parser->stack_depth == 0)                                     \
      {                                                               \
        assert(0);                                                    \
        RETURN_ERROR(INTERNAL);                                       \
      }                                                               \
    else if (parser->stack_nodes[parser->stack_depth-1].is_object)    \
      GOTO_STATE(IN_OBJECT);                                           \
    else                                                              \
      GOTO_STATE(IN_ARRAY);                                            \
  }while(0)


// Define a label and a c-level case,
// we use the label when we are midstring for efficiency,
// and the case-statement if we run out of data.
#define CASE(shortname) \
        case_##shortname: \
        case JSON_CALLBACK_PARSER_STATE_##shortname

  if (parser->whitespace_state != WHITESPACE_STATE_DEFAULT)
    {
      switch (parser->whitespace_scanner (parser, &at, end))
        {
        case SCAN_END:
          break;
        case SCAN_ERROR:
          return false;
        case SCAN_IN_VALUE:
          return true;
        }
    }

  while (at < end)
    {
      switch (parser->state)
        {
          // Toplevel states (those in between returned objects) are all
          // tagged with INTERIM.
          //
          // The following is the initial state and is the start of each line in
          // some formats.
          CASE(INTERIM):
            SKIP_WS();
            if (*at == '{')
              {
                // push object marker onto stack
                PUSH_OBJECT();
                GOTO_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                // push object marker onto stack
                PUSH_OBJECT();
                GOTO_STATE(IN_ARRAY);
              }
            else if (*at == ',')
              {
                if (parser->options.permit_trailing_commas)
                  {
                    at++;
                    GOTO_STATE(INTERIM_GOT_COMMA);
                  }
              }
            else if (is_flat_value_char (parser, *at))
              {
                if (!parser->options.permit_bare_values)
                  RETURN_ERROR(EXPECTED_STRUCTURED_VALUE);
                parser->flat_value_state = initial_flat_value_state (*at);
                at++;
                GOTO_STATE(INTERIM_VALUE);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(INTERIM_EXPECTING_COMMA):
            if (*at == ',') 
              {
                at++;
                GOTO_STATE(INTERIM);
              }
            else if (IS_SPACE(*at))
              {
                at++;
                continue;
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

          CASE(INTERIM_GOT_COMMA):
            switch (parser->whitespace_scanner (parser, &at, end))
              {
                case SCAN_END:
                  break;

                case SCAN_ERROR:
                  return false;

                case SCAN_IN_VALUE:
                  GOTO_STATE(INTERIM_VALUE);
              }
            if (*at == ',' && parser->options.ignore_multiple_commas)
              {
                at++;
                GOTO_STATE(INTERIM_GOT_COMMA);
              }
            if (*at == '[')
              {
                at++;
                PUSH_ARRAY();
              }
            if (*at == '{')
              {
                // push object marker onto stack
                PUSH_OBJECT();
                GOTO_STATE(IN_OBJECT);
              }
            if (*at == ',')
              {
                if (parser->options.permit_trailing_commas)
                  {
                    at++;
                    GOTO_STATE(INTERIM_GOT_COMMA);
                  }
              }
            if (is_flat_value_char (parser, *at))
              {
                if (!parser->options.permit_bare_values)
                  RETURN_ERROR(EXPECTED_STRUCTURED_VALUE);
                parser->flat_value_state = initial_flat_value_state (*at);
                at++;
                GOTO_STATE(INTERIM_VALUE);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(INTERIM_VALUE):
            switch (scan_flat_value (parser, &at, end))
              {
              case SCAN_END:
                GOTO_STATE(INTERIM_EXPECTING_COMMA);

              case SCAN_ERROR:
                return false;

              case SCAN_IN_VALUE:
                return true;
              }

          CASE(IN_OBJECT):
            switch (parser->whitespace_scanner (parser, &at, end))
              {
              case SCAN_IN_VALUE:
                assert(at == end);
                return true;

              case SCAN_ERROR:
                return false;

              case SCAN_END:
                break;
              }
            if (*at == '"')
              {
                at++;
                parser->flat_value_state = FLAT_VALUE_STATE_STRING;
                GOTO_STATE (IN_OBJECT_FIELDNAME);
              }
            else if (parser->options.permit_bare_fieldnames
                &&  (IS_ASCII_ALPHA (*at) || *at == '_'))
              {
                buffer_set (parser, 1, at);
                at++;
                GOTO_STATE (IN_OBJECT_BARE_FIELDNAME);
              }
            else if (*at == '}')
              {
                at++;
                POP();
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

          CASE(IN_OBJECT_FIELDNAME):
            switch (scan_flat_value (parser, &at, end))
              {
              case SCAN_END:
                GOTO_STATE(IN_OBJECT_AWAITING_COLON);

              case SCAN_IN_VALUE:
                assert(at == end);
                goto at_end;

              case SCAN_ERROR:
                return false;
              }
            break;
            
          CASE(IN_OBJECT_BARE_FIELDNAME):
            while (at < end && (IS_ASCII_ALPHA (*at) || *at == '_'))
              {
                buffer_append_c (parser, *at);
                at++;
              }
            if (at == end)
              goto at_end;
            if (IS_SPACE (*at))
              {
                do_callback_object_key (parser);
                at++;
                GOTO_STATE(IN_OBJECT_AWAITING_COLON);
              }
            else if (*at == ':')
              {
                do_callback_object_key (parser);
                at++;
                GOTO_STATE(IN_OBJECT_VALUE_SPACE);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(IN_OBJECT_AWAITING_COLON):
            while (at < end && IS_SPACE (*at))
              at++;
            if (at == end)
              goto at_end;
            if (*at == ':')
              {
                at++;
                GOTO_STATE(IN_OBJECT_VALUE_SPACE);
              }
            else
              {
                RETURN_ERROR(EXPECTED_COLON);
              }
            break;

          CASE(IN_OBJECT_VALUE_SPACE):
            if (*at == '{')
              {
                PUSH_OBJECT();
                at++;
                GOTO_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                PUSH_ARRAY();
                at++;
                GOTO_STATE(IN_ARRAY);
              }
            else if (is_flat_value_char (parser, *at))
              {
                parser->flat_value_state = initial_flat_value_state (*at);
                at++;
                GOTO_STATE(IN_OBJECT_VALUE);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

            // this state is only used for non-structured values; otherwise, we use the stack
          CASE(IN_OBJECT_VALUE):
            switch (scan_flat_value (parser, &at, end))
              {
              case SCAN_END:
                GOTO_STATE(IN_OBJECT_AWAITING_COLON);

              case SCAN_IN_VALUE:
                assert(at == end);
                goto at_end;

              case SCAN_ERROR:
                return false;
              }

          CASE(IN_ARRAY):
            if (*at == '{')
              {
                at++;
                PUSH_OBJECT();
                GOTO_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                at++;
                PUSH_ARRAY();
                GOTO_STATE(IN_ARRAY);
              }
            else if (*at == ']')
              {
                POP();
              }
            else if (*at == ',' && parser->options.permit_trailing_commas)
              GOTO_STATE(IN_ARRAY_GOT_COMMA);
            else if (is_flat_value_char (parser, *at))
              {
                parser->flat_value_state = initial_flat_value_state (*at);
                at++;
                GOTO_STATE(IN_ARRAY_VALUE);
              }
            else 
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(IN_ARRAY_VALUE):
            switch (scan_flat_value (parser, &at, end))
              {
              case SCAN_ERROR:
                return false;
              case SCAN_END:
                GOTO_STATE(IN_ARRAY_EXPECTING_COMMA);

              case SCAN_IN_VALUE:
                assert(at == end);
                return true;
              }

          CASE(IN_ARRAY_EXPECTING_COMMA):
            if (*at == ',')
              {
                at++;
                GOTO_STATE(IN_ARRAY_GOT_COMMA);
              }
            else if (*at == ']')
              {
                POP();
              }
            else if (parser->options.ignore_missing_commas && IS_SPACE(*at))
              {
                GOTO_STATE(IN_ARRAY);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

          CASE(IN_ARRAY_GOT_COMMA):
            switch (parser->whitespace_scanner (parser, &at, end))
              {
              case SCAN_END:
                break;
              case SCAN_IN_VALUE:
                assert(at==end);
                return true;
              case SCAN_ERROR:
                return false;
              }

            if (*at == ']')
              {
                at++;
                POP();
              }
            else if (*at == '{')
              {
                at++;
                PUSH_OBJECT();
              }
            else if (*at == '[')
              {
                at++;
                PUSH_ARRAY();
              }
            else if (is_flat_value_char (parser, *at))
              {
                parser->flat_value_state = initial_flat_value_state (*at);
                at++;
                GOTO_STATE(IN_ARRAY_VALUE);
              }
            else if (*at == ',' && parser->options.ignore_missing_commas)
              {
                at++;
                GOTO_STATE(IN_ARRAY_GOT_COMMA);
              }
            else
              {
                parser->error_char = *at;
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
        }
    }

at_end:
  return true;

#undef SKIP_CHAR_TYPE
#undef IS_SPACE
#undef SKIP_WS
#undef RETURN_ERROR
#undef GOTO_STATE
#undef PUSH
#undef PUSH_OBJECT
#undef PUSH_ARRAY
#undef POP
#undef CASE
}

JSON_CALLBACK_PARSER_FUNC_DEF
bool
json_callback_parser_end_feed (JSON_CallbackParser *parser)
{
  switch (parser->state)
    {
    case JSON_CALLBACK_PARSER_STATE_INTERIM:
      return true;
    case JSON_CALLBACK_PARSER_STATE_INTERIM_EXPECTING_COMMA:
      return true;
    case JSON_CALLBACK_PARSER_STATE_INTERIM_GOT_COMMA:
      if (parser->options.permit_trailing_commas)
        return true;
      else
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_TRAILING_COMMA;
          return false;
        }
        
    case JSON_CALLBACK_PARSER_STATE_INTERIM_VALUE:
      if (flat_value_can_terminate (parser))
        {
          if (flat_value_state_is_string (parser->flat_value_state))
            {
              do_callback_string(parser);
            }
          else if (flat_value_state_is_number (parser->flat_value_state))
            {
              do_callback_number(parser);
            }
          else if (parser->flat_value_state == FLAT_VALUE_STATE_IN_TRUE && parser->flat_len == 4)
            {
              do_callback_boolean(parser, true);
            }
          else if (parser->flat_value_state == FLAT_VALUE_STATE_IN_FALSE && parser->flat_len == 5)
            {
              do_callback_boolean(parser, false);
            }
          else if (parser->flat_value_state == FLAT_VALUE_STATE_IN_NULL && parser->flat_len == 4)
            {
              do_callback_null(parser);
            }
          else
            {
              assert(false);
            }
          return true;
        }
      else
        {
          parser->error_code = JSON_CALLBACK_PARSER_ERROR_PARTIAL_RECORD;
          return false;
        }

    case JSON_CALLBACK_PARSER_STATE_IN_ARRAY:
    case JSON_CALLBACK_PARSER_STATE_IN_ARRAY_EXPECTING_COMMA:
    case JSON_CALLBACK_PARSER_STATE_IN_ARRAY_GOT_COMMA:
    case JSON_CALLBACK_PARSER_STATE_IN_ARRAY_VALUE:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT_AWAITING_COLON:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT_FIELDNAME:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT_BARE_FIELDNAME:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT_VALUE:
    case JSON_CALLBACK_PARSER_STATE_IN_OBJECT_VALUE_SPACE:
      parser->error_code = JSON_CALLBACK_PARSER_ERROR_PARTIAL_RECORD;
      return false;
    }
}


JSON_CALLBACK_PARSER_FUNC_DEF
JSON_CallbackParserError
json_callback_parser_get_error_info(JSON_CallbackParser *parser)
{
  return parser->error_code;
}
