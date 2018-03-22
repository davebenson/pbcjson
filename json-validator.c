#include "json-validator.h"
#include <assert.h>
#include <stdlib.h>


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
  JSON_VALIDATOR_STATE_INTERIM,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_EXPECTING_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_GOT_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_INITIAL,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_VALUE,  // flat_value_state is valid
  JSON_VALIDATOR_STATE_INTERIM_EXPECTING_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_GOT_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_EXPECTING_EOL,
  JSON_VALIDATOR_STATE_INTERIM_EXPECTING_EOF,
  JSON_VALIDATOR_STATE_INTERIM_VALUE,
  JSON_VALIDATOR_STATE_IN_ARRAY,
  JSON_VALIDATOR_STATE_IN_ARRAY_EXPECTING_COMMA,
  JSON_VALIDATOR_STATE_IN_ARRAY_GOT_COMMA,
  JSON_VALIDATOR_STATE_IN_ARRAY_VALUE,   // flat_value_state is valid
  JSON_VALIDATOR_STATE_IN_OBJECT,
  JSON_VALIDATOR_STATE_IN_OBJECT_AWAITING_COLON,
  JSON_VALIDATOR_STATE_IN_OBJECT_FIELDNAME,     // flat_value_state is valid
  JSON_VALIDATOR_STATE_IN_OBJECT_BARE_FIELDNAME,
  JSON_VALIDATOR_STATE_IN_OBJECT_VALUE, // flat_value_state is valid
  JSON_VALIDATOR_STATE_IN_OBJECT_VALUE_SPACE,
} JSON_ValidatorState;

#define state_is_interim(state) ((state) <= JSON_VALIDATOR_STATE_INTERIM_EXPECTING_EOL)

// The flat-value-state is used to share the many substates possible for literal values.
// The following states use this substate:
//    JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_VALUE
//    JSON_VALIDATOR_STATE_IN_OBJECT_VALUE_SPACE
typedef enum
{
  // literal strings - parse states
  FLAT_VALUE_STATE_STRING,
  FLAT_VALUE_STATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U,
  FLAT_VALUE_STATE_IN_UTF8_CHAR,        // validator->utf8_state gives details
  FLAT_VALUE_STATE_GOT_BACKSLASH_0,
  FLAT_VALUE_STATE_IN_BACKSLASH_UTF8,
  FLAT_VALUE_STATE_IN_BACKSLASH_CR,
  FLAT_VALUE_STATE_IN_BACKSLASH_X,
  FLAT_VALUE_STATE_IN_BACKSLASH_U,

  // literal numbers - parse states
  FLAT_VALUE_STATE_GOT_SIGN,
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

  // literal barewords - true, false, null
  FLAT_VALUE_STATE_IN_NULL,
  FLAT_VALUE_STATE_IN_TRUE,
  FLAT_VALUE_STATE_IN_FALSE,

} FlatValueState;

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


typedef ScanResult (*WhitespaceScannerFunc) (JSON_Validator *validator,
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

typedef struct JSON_Validator_StackNode {
  uint8_t is_object : 1;                        // otherwise, it's an array
} JSON_Validator_StackNode;

struct JSON_Validator {
  JSON_Validator_Options options;

  // NOTE: The stack doesn't include the outside array for ARRAY_OF_OBJECTS
  unsigned stack_depth;
  JSON_Validator_StackNode *stack_nodes;

  JSON_ValidatorState state;
  JSON_ValidatorError error_code;

  // this is for strings and numbers; we also use it for quoted object field names.
  FlatValueState flat_value_state;

  WhitespaceScannerFunc whitespace_scanner;
  WhitespaceState whitespace_state;

  // Meaning of these two members depends on flat_value_state:
  //    * backslash sequence or UTF8 partial character
  unsigned flat_len;
  char flat_data[8];

  UTF8State utf8_state;
};

static ScanResult
scan_whitespace_json   (JSON_Validator *validator,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
  const uint8_t *at = *p_at;
  (void) validator;
  while (at < end && IS_SPACE (*at))
    at++;
  *p_at = at;
  return SCAN_END;
}
static ScanResult
scan_whitespace_json5  (JSON_Validator *validator,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
#define CASE(shortname) case_##shortname: case WHITESPACE_STATE_##shortname
#define GOTO_WHITESPACE_STATE(shortname)                      \
do {                                                          \
  validator->whitespace_state = WHITESPACE_STATE_##shortname; \
  if (at == end) {                                            \
    *p_at = at;                                               \
    return SCAN_IN_VALUE;                                     \
  }                                                           \
  goto case_##shortname;                                      \
} while(0)
  const uint8_t *at = *p_at;
  (void) validator;
  switch (validator->whitespace_state)
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
          validator->error_code = JSON_VALIDATOR_ERROR_UNEXPECTED_CHAR;
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
  validator->error_code = JSON_VALIDATOR_ERROR_UNEXPECTED_CHAR;
  return SCAN_ERROR;
}
static ScanResult
scan_whitespace_generic(JSON_Validator *validator,
                        const uint8_t **p_at,
                        const uint8_t  *end)
{
#define CASE(shortname) case_##shortname: case WHITESPACE_STATE_##shortname
#define GOTO_WHITESPACE_STATE(shortname)                      \
do {                                                          \
  validator->whitespace_state = WHITESPACE_STATE_##shortname; \
  if (at == end) {                                            \
    *p_at = at;                                               \
    return SCAN_IN_VALUE;                                     \
  }                                                           \
  goto case_##shortname;                                      \
} while(0)
  const uint8_t *at = *p_at;
  (void) validator;
  switch (validator->whitespace_state)
    {
    CASE(DEFAULT):
      while (at < end && IS_SPACE(*at))
        at++;
      if (*at == '/' && (validator->options.ignore_single_line_comments
                       || validator->options.ignore_multi_line_comments))
        {
          at++;
          GOTO_WHITESPACE_STATE(SLASH);
        }
      else if (*at == 0xe1 && validator->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E1);
        }
      else if (*at == 0xe2 && validator->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E2);
        }
      else if (*at == 0xe3 && validator->options.ignore_unicode_whitespace)
        {
          at++;
          GOTO_WHITESPACE_STATE(UTF8_E3);
        }
      else
        goto nonws_nonascii;
    CASE(SLASH):
      if (*at == '*' && validator->options.ignore_multi_line_comments)
        {
          at++;
          GOTO_WHITESPACE_STATE(MULTILINE_COMMENT);
        }
      else if (*at == '/' && validator->options.ignore_single_line_comments)
        {
          at++;
          GOTO_WHITESPACE_STATE(EOL_COMMENT);
        }
      else
        {
          *p_at = at;
          validator->error_code = JSON_VALIDATOR_ERROR_UNEXPECTED_CHAR;
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
  validator->error_code = JSON_VALIDATOR_ERROR_UNEXPECTED_CHAR;
  return SCAN_ERROR;
}


size_t
json_validator_options_get_memory_size (const JSON_Validator_Options *options)
{
  return sizeof (JSON_Validator)
       + options->max_stack_depth * sizeof (JSON_Validator_StackNode);
}

// note that JSON_Validator allocates no further memory, so this can 
// be embedded somewhere.
JSON_Validator *
json_validator_init (void                         *memory,
                     const JSON_Validator_Options *options)
{
  JSON_Validator *validator = memory;
  validator->options = *options;
  validator->stack_depth = 0;
  validator->stack_nodes = (JSON_Validator_StackNode *) (validator + 1);
  validator->state = JSON_VALIDATOR_STATE_INTERIM;
  validator->error_code = JSON_VALIDATOR_ERROR_NONE;

  // select optimized whitespace scanner
  if (!options->ignore_single_line_comments
   && !options->ignore_multi_line_comments
   && !options->disallow_extra_whitespace
   && !options->ignore_unicode_whitespace)
    validator->whitespace_scanner = scan_whitespace_json;
  else
  if ( options->ignore_single_line_comments
   &&  options->ignore_multi_line_comments
   && !options->disallow_extra_whitespace
   &&  options->ignore_unicode_whitespace)
    validator->whitespace_scanner = scan_whitespace_json5;
  else
    validator->whitespace_scanner = scan_whitespace_generic;
    
  return validator;
}

static inline ScanResult
utf8_validate_char (JSON_Validator *validator,
                    const uint8_t **p_at,
                    const uint8_t *end)
{
  const uint8_t *at = *p_at;
  if ((*at & 0xe0) == 0xc0)
    {
      if ((*at & 0x1f) < 2)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // 2 byte sequence
      if (at + 1 == end)
        {
          validator->utf8_state = UTF8_STATE_2_1;
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
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
          validator->utf8_state = ((at[0] & 0x0f) != 0)
                                ? UTF8_STATE_3_1_got_nonzero
                                : UTF8_STATE_3_1_got_zero;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if ((at[0] & 0x0f) == 0 && (at[1] & 0x40) == 0)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      if (at + 2 == end)
        {
          validator->utf8_state = UTF8_STATE_3_2;
          return SCAN_IN_VALUE;
        }
      if ((at[2] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
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
          validator->utf8_state = ((at[0] & 0x07) != 0)
                                ? UTF8_STATE_4_1_got_nonzero
                                : UTF8_STATE_4_1_got_zero;
          *p_at = at + 1;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if ((at[0] & 0x07) == 0 && (at[1] & 0x30) == 0)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      if (at + 2 == end)
        {
          validator->utf8_state = UTF8_STATE_4_2;
        }
      if ((at[2] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if (at + 3 == end)
        {
          validator->utf8_state = UTF8_STATE_4_3;
        }
      if ((at[3] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 4;
      return SCAN_END;
    }
  else
    {
      // invalid initial utf8-byte
      validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_INITIAL_BYTE;
      return SCAN_ERROR;
    }
}
static inline ScanResult
utf8_continue_validate_char (JSON_Validator *validator,
                             const uint8_t **p_at,
                             const uint8_t *end)
{
  const uint8_t *at = *p_at;
  switch (validator->utf8_state)
    {
    case UTF8_STATE_2_1:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      *p_at = at + 1;
      return SCAN_END;

    case UTF8_STATE_3_1_got_zero:
      if ((at[0] & 0x40) == 0)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // fall through

    case UTF8_STATE_3_1_got_nonzero:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          return SCAN_ERROR;
        }
      if (at + 1 == end)
        {
          validator->utf8_state = UTF8_STATE_3_2;
          *p_at = at + 1;
          return SCAN_IN_VALUE;
        }
      if ((at[1] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          validator->utf8_state = UTF8_STATE_3_1_got_nonzero;
          return SCAN_ERROR;
        }
      *p_at = at + 2;
      return SCAN_END;

    case UTF8_STATE_3_2:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          validator->utf8_state = UTF8_STATE_3_2;
          return SCAN_ERROR;
        }
      *p_at = at + 1;
      return SCAN_END;

    case UTF8_STATE_4_1_got_zero:
      if ((at[0] & 0x30) == 0)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_OVERLONG;
          return SCAN_ERROR;
        }
      // fall through
    case UTF8_STATE_4_1_got_nonzero:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          validator->utf8_state = UTF8_STATE_4_1_got_nonzero;
          return SCAN_ERROR;
        }
      at++;
      if (at == end)
        {
          *p_at = at;
          validator->utf8_state = UTF8_STATE_4_2;
          return SCAN_IN_VALUE;
        }
      // fall-through

    case UTF8_STATE_4_2:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          validator->utf8_state = UTF8_STATE_4_2;
          return SCAN_ERROR;
        }
      at++;
      if (at == end)
        {
          *p_at = at;
          validator->utf8_state = UTF8_STATE_4_3;
          return SCAN_IN_VALUE;
        }
      // fall-through

    case UTF8_STATE_4_3:
      if ((at[0] & 0xc0) != 0x80)
        {
          validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_TRAILING_BYTE;
          validator->utf8_state = UTF8_STATE_4_3;
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
is_flat_value_char (JSON_Validator *validator,
                    char            c)
{
  if (c == '"') return 1;
  if ('0' <= c && c <= '9') return 1;
  if (c == '\'' && validator->options.permit_single_quote_strings) return 1;
  if (c == '.' && validator->options.permit_leading_decimal_point) return 1;
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
generic_bareword_handler (JSON_Validator *validator,
                          const uint8_t**p_at,
                          const uint8_t *end,
                          const char    *bareword,
                          size_t         bareword_len)
{
  const uint8_t *at = *p_at;
  while (at < end
      && validator->flat_len < bareword_len
      && (char)(*at) == bareword[validator->flat_len])
    {
      validator->flat_len += 1;
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
  validator->error_code = JSON_VALIDATOR_ERROR_BAD_BAREWORD;
  *p_at = at;
  return SCAN_ERROR;
}

static ScanResult
scan_flat_value (JSON_Validator *validator,
                 const uint8_t **p_at,
                 const uint8_t  *end)
{
  const uint8_t *at = *p_at;
  char end_quote_char;

#define FLAT_VALUE_GOTO_STATE(st_shortname) \
  do{ \
    FLAT_VALUE_STATE_REPLACE_ENUM_BITS (validator->flat_value_state, FLAT_VALUE_STATE_##st_shortname); \
    if (at == end) \
      return SCAN_IN_VALUE; \
    goto case_##st_shortname; \
  }while(0)

  switch (FLAT_VALUE_STATE_MASKED (validator->flat_value_state))
    {
    case_STRING:
    case FLAT_VALUE_STATE_STRING:
      end_quote_char = flat_value_state_to_end_quote_char (validator->flat_value_state);
      while (at < end)
        {
          if (*at == '\\')
            {
              at++;
              validator->flat_len = 0;
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
              at++;
              continue;
            }
          else
            {
              switch (utf8_validate_char (validator, &at, end))
                {
                case SCAN_END:
                  break;
                case SCAN_IN_VALUE:
                  // note that validator->utf8_state is set
                  // when we get this return-value.
                  assert(at == end);
                  FLAT_VALUE_STATE_REPLACE_ENUM_BITS(validator->flat_value_state,
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
        case 'r': case 't': case 'n': case 'b': case 'v': case 'f':
        case '"': case '\\': case '\'':
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);

        case '0':      
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_BACKSLASH_0);

        case '\n':
          if (!validator->options.permit_line_continuations_in_strings)
            {
              validator->error_code = JSON_VALIDATOR_ERROR_QUOTED_NEWLINE;
              return SCAN_ERROR;
            }
          at++;
          FLAT_VALUE_GOTO_STATE(STRING);

        case '\r':
          // must also accept CRLF = 13,10
          if (!validator->options.permit_line_continuations_in_strings)
            {
              validator->error_code = JSON_VALIDATOR_ERROR_QUOTED_NEWLINE;
              return SCAN_ERROR;
            }
          at++;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_CR);

        case 'x': case 'X':
          if (!validator->options.permit_backslash_x)
            {
              validator->error_code = JSON_VALIDATOR_ERROR_BACKSLASH_X_NOT_ALLOWED;
              return SCAN_ERROR;
            }
          at++;
          validator->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_X);

        case 'u': case 'U':
          at++;
          validator->flat_len = 0;
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
                  validator->error_code = JSON_VALIDATOR_ERROR_UTF8_BAD_INITIAL_BYTE;
                  *p_at = at;
                  return SCAN_ERROR;
                }
              else
                {
                  // multi-byte passthrough character
                  validator->flat_data[0] = *at;
                  validator->flat_len = 1;
                  switch (utf8_validate_char (validator, &at, end))
                    {
                    case SCAN_END:
                      FLAT_VALUE_GOTO_STATE(STRING);

                    case SCAN_IN_VALUE:
                      *p_at = at;
                      validator->flat_value_state = FLAT_VALUE_STATE_IN_BACKSLASH_UTF8;
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
      while (validator->flat_len < 2 && at < end && IS_HEX_DIGIT(*at))
        {
          validator->flat_len++;
          at++;
        }
      if (validator->flat_len == 2)
        FLAT_VALUE_GOTO_STATE(STRING);
      else if (at == end)
        {
          *p_at = at;
          return SCAN_IN_VALUE;
        }
      else
        {
          *p_at = at;
          validator->error_code = JSON_VALIDATOR_ERROR_EXPECTED_HEX_DIGIT;
          return SCAN_ERROR;
        }

    case_IN_BACKSLASH_U:
    case FLAT_VALUE_STATE_IN_BACKSLASH_U:
      while (validator->flat_len < 4 && at < end && IS_HEX_DIGIT(*at))
        {
          validator->flat_data[validator->flat_len] = *at;
          validator->flat_len++;
          at++;
        }
      if (validator->flat_len == 4)
        {
          validator->flat_data[4] = 0;
          uint32_t code = strtoul (validator->flat_data, NULL, 16);
          if (IS_HI_SURROGATE(code))
            {
              FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE);
            }
          else if (IS_LO_SURROGATE(code))
            {
              validator->error_code = JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR;
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
          validator->error_code = JSON_VALIDATOR_ERROR_EXPECTED_HEX_DIGIT;
          return SCAN_ERROR;
        }
    case_GOT_HI_SURROGATE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE:
      if (*at == '\\')
        FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE);
      else
        {
          *p_at = at;
          validator->error_code = JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR;
          return SCAN_ERROR;
        }
    
    case_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
      if (*at == 'u' || *at == 'U')
        {
          validator->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U);
        }
      else
        {
          *p_at = at;
          validator->error_code = JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR;
          return SCAN_ERROR;
        }

    case_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U:
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE_U:
      while (validator->flat_len < 4 && at < end && IS_HEX_DIGIT(*at))
        {
          validator->flat_data[validator->flat_len] = *at;
          validator->flat_len++;
          at++;
        }
      if (validator->flat_len == 4)
        {
          validator->flat_data[4] = 0;
          uint32_t code = strtoul (validator->flat_data, NULL, 16);
          if (IS_LO_SURROGATE(code))
            FLAT_VALUE_GOTO_STATE(STRING);
          else
            {
              *p_at = at;
              validator->error_code = JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR;
              return SCAN_ERROR;
            }
        }
      else if (at < end)
        {
          *p_at = at;
          validator->error_code = JSON_VALIDATOR_ERROR_EXPECTED_HEX_DIGIT;
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
          validator->error_code = JSON_VALIDATOR_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL;
          return SCAN_ERROR;
        }
      FLAT_VALUE_GOTO_STATE(STRING);

    //case_IN_BACKSLASH_UTF8:
    case FLAT_VALUE_STATE_IN_BACKSLASH_UTF8:
      switch (utf8_continue_validate_char (validator, &at, end))
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
      switch (utf8_continue_validate_char (validator, &at, end))
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
      else if (*at == '.' && validator->options.permit_leading_decimal_point)
        {
          at++;
          FLAT_VALUE_GOTO_STATE(GOT_LEADING_DECIMAL_POINT);
        }
      else
        {
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
          *p_at = at;
          return SCAN_ERROR;
        }

    case_GOT_0:
    case FLAT_VALUE_STATE_GOT_0:
      if ((*at == 'x' || *at == 'X') && validator->options.permit_hex_numbers)
        {
          at++;
          FLAT_VALUE_GOTO_STATE(IN_HEX_EMPTY);
        }
      else if (('0' <= *at && *at <= '7') && validator->options.permit_octal_numbers)
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
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
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
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }

    case_IN_HEX_EMPTY:
    case FLAT_VALUE_STATE_IN_HEX_EMPTY:
      if (!IS_HEX_DIGIT (*at))
        {
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
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
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
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
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }
      

    case_GOT_LEADING_DECIMAL_POINT:
    case FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT:
      if (!IS_DIGIT (*at))
        {
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
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
          validator->error_code = JSON_VALIDATOR_ERROR_BAD_NUMBER;
          return SCAN_ERROR;
        }
      *p_at = at;
      return SCAN_END;

    //case_IN_NULL:
    case FLAT_VALUE_STATE_IN_NULL:
      *p_at = at;
      return generic_bareword_handler (validator, p_at, end, "null", 4);

    //case_IN_TRUE:
    case FLAT_VALUE_STATE_IN_TRUE:
      *p_at = at;
      return generic_bareword_handler (validator, p_at, end, "true", 4);

    //case_IN_FALSE:
    case FLAT_VALUE_STATE_IN_FALSE:
      *p_at = at;
      return generic_bareword_handler (validator, p_at, end, "false", 5);
    }

#undef FLAT_VALUE_GOTO_STATE
  assert(0);
  return SCAN_ERROR;
}

JSON_VALIDATOR_FUNC_DEF
JSON_ValidatorResult
json_validator_feed (JSON_Validator *validator,
                     size_t          len,
                     const uint8_t  *data,
                     size_t         *used)
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
    *used = at - data;                                                     \
    return JSON_VALIDATOR_RESULT_ERROR;                                    \
  }while(0)

#define RETURN_ERROR(error_code_shortname)                                 \
  do{                                                                      \
    validator->error_code = JSON_VALIDATOR_ERROR_ ## error_code_shortname; \
    *used = at - data;                                                     \
    return JSON_VALIDATOR_RESULT_ERROR;                                    \
  }while(0)

#define SET_STATE(state_shortname)                                    \
  do{                                                                 \
    validator->state = JSON_VALIDATOR_STATE_ ## state_shortname;      \
    if (at == end)                                                    \
      goto at_end;                                                    \
    goto case_##state_shortname;                                      \
  }while(0)

#define PUSH(is_obj)                                                  \
  do{                                                                 \
    if (validator->stack_depth == validator->options.max_stack_depth) \
      RETURN_ERROR(STACK_DEPTH_EXCEEDED);                             \
    JSON_Validator_StackNode *n = validator->stack_nodes + validator->stack_depth;\
    n->is_object = (is_obj);                                          \
    ++validator->stack_depth;                                         \
  }while(0)
  
#define PUSH_OBJECT() PUSH(1)
#define PUSH_ARRAY() PUSH(0)

#define POP()                                                         \
  do{                                                                 \
    assert(validator->stack_depth > 0);                               \
    --validator->stack_depth;                                         \
    if (validator->stack_depth == 0)                                  \
      {                                                               \
        switch (validator->options.encapsulation)                     \
          {                                                           \
          case JSON_VALIDATOR_ENCAPSULATION_ARRAY:                    \
            SET_STATE(INTERIM_IN_ARRAY_EXPECTING_COMMA);              \
          case JSON_VALIDATOR_ENCAPSULATION_LINE_BY_LINE:             \
            SET_STATE(INTERIM_EXPECTING_EOL);                         \
          case JSON_VALIDATOR_ENCAPSULATION_WHITESPACE_SEP:           \
            SET_STATE(INTERIM);                                       \
          case JSON_VALIDATOR_ENCAPSULATION_COMMA_SEP:                \
            SET_STATE(INTERIM_EXPECTING_COMMA);                       \
          case JSON_VALIDATOR_ENCAPSULATION_SINGLE_OBJECT:            \
            SET_STATE(INTERIM_EXPECTING_EOF);                         \
          }                                                           \
        assert(0);                                                    \
        RETURN_ERROR(INTERNAL);                                       \
      }                                                               \
    else if (validator->stack_nodes[validator->stack_depth].is_object)\
      SET_STATE(IN_OBJECT);                                           \
    else                                                              \
      SET_STATE(IN_ARRAY);                                            \
  }while(0)


// Define a label and a c-level case,
// we use the label when we are midstring for efficiency,
// and the case-statement if we run out of data.
#define CASE(shortname) \
        case_##shortname: \
        case JSON_VALIDATOR_STATE_##shortname

  if (validator->whitespace_state != WHITESPACE_STATE_DEFAULT)
    {
      switch (validator->whitespace_scanner (validator, &at, end))
        {
        case SCAN_END:
          break;
        case SCAN_ERROR:
          RETURN_ERROR_CODE();
        case SCAN_IN_VALUE:
          *used = at - data;
          return state_is_interim (validator->state)
               ? JSON_VALIDATOR_RESULT_INTERIM
               : JSON_VALIDATOR_RESULT_IN_RECORD;
        }
    }

  while (at < end)
    {
      switch (validator->state)
        {
          // Toplevel states (those in between returned objects) are all
          // tagged with INTERIM.
          //
          // The following is the initial state and is the start of each line in
          // some formats.
          CASE(INTERIM):
            SKIP_WS();
            if (*at == '['
              && validator->options.encapsulation == JSON_VALIDATOR_ENCAPSULATION_ARRAY)
              {
                at++;
                SET_STATE(INTERIM_IN_ARRAY_INITIAL);
              }
            if (*at == '{')
              {
                if (validator->options.encapsulation == JSON_VALIDATOR_ENCAPSULATION_ARRAY)
                  RETURN_ERROR(EXPECTED_ARRAY_START);

                // push object marker onto stack
                PUSH_OBJECT();
                SET_STATE(IN_OBJECT);
              }
            if (*at == ',')
              {
                if (validator->options.permit_trailing_commas)
                  {
                    at++;
                    SET_STATE(INTERIM_GOT_COMMA);
                  }
              }
            if (is_flat_value_char (validator, *at))
              {
                if (!validator->options.permit_bare_values)
                  RETURN_ERROR(EXPECTED_STRUCTURED_VALUE);
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE(INTERIM_VALUE);
              }
            else
              {
                RETURN_ERROR(EXPECTED_ARRAY_START);
              }
            break;

          CASE(INTERIM_EXPECTING_COMMA):
            if (*at == ',') 
              {
                at++;
                SET_STATE(INTERIM);
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
            switch (validator->whitespace_scanner (validator, &at, end))
              {
                case SCAN_END:
                  break;

                case SCAN_ERROR:
                  return JSON_VALIDATOR_RESULT_ERROR;

                case SCAN_IN_VALUE:
                  SET_STATE(INTERIM_VALUE);
              }
            if (*at == ',' && validator->options.ignore_multiple_commas)
              {
                at++;
                SET_STATE(INTERIM_GOT_COMMA);
              }
            if (*at == '['
              && validator->options.encapsulation == JSON_VALIDATOR_ENCAPSULATION_ARRAY)
              {
                at++;
                SET_STATE(INTERIM_IN_ARRAY_INITIAL);
              }
            if (*at == '{')
              {
                if (validator->options.encapsulation == JSON_VALIDATOR_ENCAPSULATION_ARRAY)
                  RETURN_ERROR(EXPECTED_ARRAY_START);

                // push object marker onto stack
                PUSH_OBJECT();
                SET_STATE(IN_OBJECT);
              }
            if (*at == ',')
              {
                if (validator->options.permit_trailing_commas)
                  {
                    at++;
                    SET_STATE(INTERIM_GOT_COMMA);
                  }
              }
            if (is_flat_value_char (validator, *at))
              {
                if (!validator->options.permit_bare_values)
                  RETURN_ERROR(EXPECTED_STRUCTURED_VALUE);
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE(INTERIM_VALUE);
              }
            else
              {
                RETURN_ERROR(EXPECTED_ARRAY_START);
              }
            break;

            
          CASE(INTERIM_IN_ARRAY_INITIAL):
            if (*at == '[')
              {
                PUSH_ARRAY();
                SET_STATE (INTERIM_IN_ARRAY_INITIAL);
              }
            else if (*at == ']')
              {
                at++;
                SET_STATE (INTERIM_EXPECTING_EOF);
              }
            else if (*at == '{')
              {
                PUSH_OBJECT();
                at++;
                SET_STATE (IN_OBJECT);
              }
            else if (is_flat_value_char (validator, *at))
              {
                if (!validator->options.permit_bare_values)
                  {
                    RETURN_ERROR(EXPECTED_STRUCTURED_VALUE);
                  }
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE (INTERIM_IN_ARRAY_VALUE);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(INTERIM_IN_ARRAY_GOT_COMMA):
            if (*at == ']')
              {
                if (!validator->options.permit_trailing_commas)
                  RETURN_ERROR(UNEXPECTED_CHAR);
                POP();
              }
            else if (is_flat_value_char (validator, *at))
              {
                validator->flat_value_state = initial_flat_value_state(*at);
                at++;
                SET_STATE(INTERIM_IN_ARRAY_VALUE);
              }
            else if (*at == '[')
              {
                PUSH_ARRAY();
                SET_STATE (IN_ARRAY);
              }
            else if (*at == '{')
              {
                PUSH_OBJECT();
                SET_STATE (IN_OBJECT);
              }
            else if (!validator->options.ignore_multiple_commas)
              RETURN_ERROR(UNEXPECTED_CHAR);
            else
              RETURN_ERROR(UNEXPECTED_CHAR);
            break;

          // non-structured 
          CASE(INTERIM_IN_ARRAY_VALUE):
            {
            size_t fv_used = 0;
            switch (scan_flat_value (validator, &at, end))
              {
              case SCAN_END:
                at += fv_used;
                SET_STATE(INTERIM_IN_ARRAY_EXPECTING_COMMA);

              case SCAN_ERROR:
                at += fv_used;
                RETURN_ERROR_CODE();

              case SCAN_IN_VALUE:
                at = end;
                goto at_end;
              }
              assert(0);
            }

          CASE(INTERIM_IN_ARRAY_EXPECTING_COMMA):
            switch (validator->whitespace_scanner (validator, &at, end))
              {
              case SCAN_END:
                if (at == end)
                  {
                    *used = len;
                    return JSON_VALIDATOR_RESULT_INTERIM;
                  }
                break;
              case SCAN_IN_VALUE:
                *used = len;
                return JSON_VALIDATOR_RESULT_INTERIM;
              case SCAN_ERROR:
                RETURN_ERROR_CODE();
              }
            if (*at == ',')
              {
                at++;
                SET_STATE(INTERIM_IN_ARRAY_GOT_COMMA);
              }
            else if (*at == ']')
              {
                at++;
                SET_STATE(INTERIM_EXPECTING_EOF);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(INTERIM_VALUE):
            switch (scan_flat_value (validator, &at, end))
              {
              case SCAN_END:
                SET_STATE(INTERIM_EXPECTING_COMMA);

              case SCAN_ERROR:
                RETURN_ERROR_CODE();

              case SCAN_IN_VALUE:
                *used = at - data;
                return JSON_VALIDATOR_RESULT_IN_RECORD;
              }

          CASE(INTERIM_EXPECTING_EOL):
            // UGH not handling high utf8 characters
            // TODO: scan whitespace, then scan ws for newline
            while (at < end && IS_SPACE (*at))
              {
                if (*at == '\n')
                  {
                    *used = at + 1 - data;
                    return JSON_VALIDATOR_RESULT_INTERIM;
                  }
                at++;
              }

          CASE(INTERIM_EXPECTING_EOF):
            switch (validator->whitespace_scanner (validator, &at, end))
              {
              case SCAN_IN_VALUE:
              case SCAN_END:
                *used = at - data;
                return JSON_VALIDATOR_RESULT_INTERIM;
              case SCAN_ERROR:
                RETURN_ERROR_CODE();
              }
            while (at < end && IS_SPACE (*at))
              {
                if (*at == '\n')
                  {
                    *used = at + 1 - data;
                    return JSON_VALIDATOR_RESULT_INTERIM;
                  }
                at++;
              }

          CASE(IN_OBJECT):
            switch (validator->whitespace_scanner (validator, &at, end))
              {
              case SCAN_IN_VALUE:
                *used = len;
                return JSON_VALIDATOR_RESULT_IN_RECORD;

              case SCAN_ERROR:
                *used = at - data;
                return JSON_VALIDATOR_RESULT_ERROR;

              case SCAN_END:
                break;
              }
            if (*at == '"')
              {
                at++;
                validator->flat_value_state = FLAT_VALUE_STATE_STRING;
                SET_STATE (IN_OBJECT_FIELDNAME);
              }
            else if (validator->options.permit_bare_fieldnames
                &&  (IS_ASCII_ALPHA (*at) || *at == '_'))
              {
                at++;
                SET_STATE (IN_OBJECT_BARE_FIELDNAME);
              }
            else if (*at == '}')
              {
                at++;
                POP();
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

          CASE(IN_OBJECT_FIELDNAME):
            switch (scan_flat_value (validator, &at, end))
              {
              case SCAN_END:
                SET_STATE(IN_OBJECT_AWAITING_COLON);

              case SCAN_IN_VALUE:
                assert(at == end);
                goto at_end;

              case SCAN_ERROR:
                return JSON_VALIDATOR_RESULT_ERROR;
              }
            break;
            
          CASE(IN_OBJECT_BARE_FIELDNAME):
            while (at < end && (IS_ASCII_ALPHA (*at) || *at == '_'))
              at++;
            if (at == end)
              goto at_end;
            if (IS_SPACE (*at))
              {
                at++;
                SET_STATE(IN_OBJECT_AWAITING_COLON);
              }
            else if (*at == ':')
              {
                at++;
                SET_STATE(IN_OBJECT_VALUE_SPACE);
              }
            else
              {
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
                SET_STATE(IN_OBJECT_VALUE_SPACE);
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
                SET_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                PUSH_ARRAY();
                at++;
                SET_STATE(IN_ARRAY);
              }
            else if (is_flat_value_char (validator, *at))
              {
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE(IN_OBJECT_VALUE);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

            // this state is only used for non-structured values; otherwise, we use the stack
          CASE(IN_OBJECT_VALUE):
            switch (scan_flat_value (validator, &at, end))
              {
              case SCAN_END:
                SET_STATE(IN_OBJECT_AWAITING_COLON);

              case SCAN_IN_VALUE:
                assert(at == end);
                goto at_end;

              case SCAN_ERROR:
                return JSON_VALIDATOR_RESULT_ERROR;
              }

          CASE(IN_ARRAY):
            if (*at == '{')
              {
                at++;
                PUSH_OBJECT();
                SET_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                at++;
                PUSH_ARRAY();
                SET_STATE(IN_ARRAY);
              }
            else if (*at == ']')
              {
                POP();
              }
            else if (*at == ',' && validator->options.permit_trailing_commas)
              SET_STATE(IN_ARRAY_GOT_COMMA);
            else if (is_flat_value_char (validator, *at))
              {
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE(IN_ARRAY_VALUE);
              }
            else 
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
            break;

          CASE(IN_ARRAY_VALUE):
            switch (scan_flat_value (validator, &at, end))
              {
              case SCAN_ERROR:
                RETURN_ERROR_CODE();
              case SCAN_END:
                SET_STATE(IN_ARRAY_EXPECTING_COMMA);
              case SCAN_IN_VALUE:
                *used = at - data;
                return JSON_VALIDATOR_RESULT_IN_RECORD;
              }

          CASE(IN_ARRAY_EXPECTING_COMMA):
            if (*at == ',')
              {
                at++;
                SET_STATE(IN_ARRAY_GOT_COMMA);
              }
            else if (*at == ']')
              {
                POP();
              }
            else if (validator->options.ignore_missing_commas)
              {
                SET_STATE(IN_ARRAY);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }

          CASE(IN_ARRAY_GOT_COMMA):
            switch (validator->whitespace_scanner (validator, &at, end))
              {
              case SCAN_END:
                break;
              case SCAN_IN_VALUE:
                assert(at==end);
                *used = at - data;
                return JSON_VALIDATOR_RESULT_IN_RECORD;
              case SCAN_ERROR:
                *used = at - data;
                RETURN_ERROR_CODE();
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
            else if (is_flat_value_char (validator, *at))
              {
                validator->flat_value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE(IN_ARRAY_VALUE);
              }
            else if (*at == ',' && validator->options.ignore_missing_commas)
              {
                at++;
                SET_STATE(IN_ARRAY_GOT_COMMA);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHAR);
              }
        }
    }

at_end:
  *used = len;
  return state_is_interim(validator->state)
       ? JSON_VALIDATOR_RESULT_INTERIM
       : JSON_VALIDATOR_RESULT_IN_RECORD;

#undef SKIP_CHAR_TYPE
#undef IS_SPACE
#undef SKIP_WS
#undef RETURN_ERROR_CODE
#undef RETURN_ERROR
#undef SET_STATE
#undef PUSH
#undef PUSH_OBJECT
#undef PUSH_ARRAY
#undef POP
#undef CASE
}

JSON_VALIDATOR_FUNC_DEF
JSON_ValidatorResult
json_validator_end_feed (JSON_Validator *validator)
{
  switch (validator->state)
    {
    case JSON_VALIDATOR_STATE_INTERIM:
      return JSON_VALIDATOR_RESULT_INTERIM;
    case JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_EXPECTING_COMMA:
    case JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_INITIAL:
    case JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_GOT_COMMA:
    case JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_VALUE:
      validator->error_code = JSON_VALIDATOR_ERROR_CONTAINING_ARRAY_NOT_CLOSED;
      return JSON_VALIDATOR_RESULT_ERROR;
    case JSON_VALIDATOR_STATE_INTERIM_EXPECTING_COMMA:
      return JSON_VALIDATOR_RESULT_INTERIM;
    case JSON_VALIDATOR_STATE_INTERIM_GOT_COMMA:
      if (validator->options.permit_trailing_commas)
        return JSON_VALIDATOR_RESULT_INTERIM;
      else
        {
          validator->error_code = JSON_VALIDATOR_ERROR_TRAILING_COMMA;
          return JSON_VALIDATOR_RESULT_ERROR;
        }
        
    case JSON_VALIDATOR_STATE_INTERIM_EXPECTING_EOL:
    case JSON_VALIDATOR_STATE_INTERIM_EXPECTING_EOF:
      return JSON_VALIDATOR_RESULT_INTERIM;

    case JSON_VALIDATOR_STATE_INTERIM_VALUE:
      if (flat_value_can_terminate (validator))
        return JSON_VALIDATOR_RESULT_INTERIM;
      else
        {
          validator->error_code = JSON_VALIDATOR_ERROR_PARTIAL_RECORD;
          return JSON_VALIDATOR_RESULT_ERROR;
        }

    case JSON_VALIDATOR_STATE_IN_ARRAY:
    case JSON_VALIDATOR_STATE_IN_ARRAY_EXPECTING_COMMA:
    case JSON_VALIDATOR_STATE_IN_ARRAY_GOT_COMMA:
    case JSON_VALIDATOR_STATE_IN_ARRAY_VALUE:
    case JSON_VALIDATOR_STATE_IN_OBJECT:
    case JSON_VALIDATOR_STATE_IN_OBJECT_AWAITING_COLON:
    case JSON_VALIDATOR_STATE_IN_OBJECT_FIELDNAME:
    case JSON_VALIDATOR_STATE_IN_OBJECT_BARE_FIELDNAME:
    case JSON_VALIDATOR_STATE_IN_OBJECT_VALUE:
    case JSON_VALIDATOR_STATE_IN_OBJECT_VALUE_SPACE:
      validator->error_code = JSON_VALIDATOR_ERROR_PARTIAL_RECORD;
      return JSON_VALIDATOR_RESULT_ERROR;
    }
}


JSON_VALIDATOR_FUNC_DEF
JSON_ValidatorError
json_validator_get_error_info(JSON_Validator *validator)
{
  return validator->error_code;
}
