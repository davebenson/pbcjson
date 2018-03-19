#include "json-validator.h"
#include <assert.h>


typedef enum
{
  JSON_VALIDATOR_STATE_INTERIM,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_COMMA_NOT_ALLOWED,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_EXPECTING_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_GOT_COMMA,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_INITIAL,
  JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_VALUE,
  JSON_VALIDATOR_STATE_IN_ARRAY,
  JSON_VALIDATOR_STATE_IN_FLAT_VALUE,
  JSON_VALIDATOR_STATE_IN_OBJECT,
  JSON_VALIDATOR_STATE_IN_OBJECT_AWAITING_COLON,
  JSON_VALIDATOR_STATE_IN_OBJECT_FIELDNAME,
  JSON_VALIDATOR_STATE_IN_OBJECT_BARE_FIELDNAME,
  JSON_VALIDATOR_STATE_IN_OBJECT_INITIAL,
  JSON_VALIDATOR_STATE_IN_OBJECT_VALUE,
  JSON_VALIDATOR_STATE_IN_OBJECT_VALUE_SPACE
} JSON_ValidatorState;


// The flat-value-state is used to share the many substates possible for literal values.
// The following states use this substate:
//    JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY_VALUE
//    JSON_VALIDATOR_STATE_IN_FLAT_VALUE,
typedef enum
{
  // literal strings - parse states
  FLAT_VALUE_STATE_STRING,
  FLAT_VALUE_STATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE,
  FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE,
  FLAT_VALUE_STATE_IN_UTF8_CHAR,        // validator->utf8_state gives details
  FLAT_VALUE_STATE_GOT_BACKSLASH_0,

  // literal numbers - parse states
  FLAT_VALUE_STATE_GOT_SIGN,
  FLAT_VALUE_STATE_GOT_0,
  FLAT_VALUE_STATE_IN_DIGITS,
  FLAT_VALUE_STATE_IN_OCTAL,
  FLAT_VALUE_STATE_IN_HEX,
  FLAT_VALUE_STATE_POST_E,
  FLAT_VALUE_STATE_POST_E_PM_OR_DIGITS,
  FLAT_VALUE_STATE_POST_E_DIGITS,
  FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT,

  // literal barewords - true, false, null
  FLAT_VALUE_STATE_IN_NULL,
  FLAT_VALUE_STATE_IN_TRUE,
  FLAT_VALUE_STATE_IN_FALSE,

} FlatValueState;

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

  // Meaning of these two members depends on flat_value_state:
  //    * backslash sequence or UTF8 partial character
  unsigned flat_len;
  char flat_data[4];

  UTF8State utf8_state;
};

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

static inline int
is_flat_value_char (JSON_Validator *validator,
                    char            c)
{
  if (c == '"') return 1;
  if ('0' <= c && c <= '9') return 1;
  if (c == '\'' && validator->options.permit_single_quote_strings) return 1;
  if (c == '.' && validator->options.permit_leading_decimal_numbers) return 1;
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
scan_flat_value (JSON_Validator *validator,
                 size_t          len,
                 const uint8_t  *data,
                 size_t         *used)
{
  const uint8_t *at = data, *end = data + len;
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
              *used = (at + 1) - data;
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
                  *used = len;
                  return SCAN_IN_VALUE;
                case SCAN_ERROR:
                  return SCAN_ERROR;
                }
            }
        }
      return SCAN_IN_VALUE;

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
          if (at == end)
            {
              validator->flat_value_state = FLAT_VALUE_STATE_GOT_BS_13;
              return SCAN_IN_VALUE;
            }
          if (*at == '\n')
            {
              at++;
              FLAT_VALUE_GOTO_STATE(STRING);
            }

        case 'x': case 'X':
          if (validator->options.permit_backslash_x)
            {
              validator->error_code = JSON_VALIDATOR_ERROR_BS_X_NOT_ALLOWED;
              return SCAN_ERROR;
            }
          at++;
          validator->flat_len = 0;
          FLAT_VALUE_GOTO_STATE(IN_BACKSLASH_X);

        case 'u': case 'U':
          ...
          ... remember to check for hi-surrogate

        // TODO: for JSON5, support line-separator and paragraph separator (0x2028,x2029)
        case ...

        case ... // ANY OTHER CHARACTER MAY BE BACKSLASHED, implying that multi-byte characters
                 // are included (JSON5 spec)
        }

    case FLAT_VALUE_STATE_IN_BACKSLASH_X:
      ...
    case FLAT_VALUE_STATE_IN_BACKSLASH_U:
      ...
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE:
      ...
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_SEQUENCE:
      ...
    case FLAT_VALUE_STATE_GOT_HI_SURROGATE_IN_BACKSLASH_U_SEQUENCE:
      ...
    case FLAT_VALUE_STATE_IN_UTF8_CHAR:
      ...

    case FLAT_VALUE_STATE_GOT_SIGN:
      ...
    case FLAT_VALUE_STATE_GOT_0:
      if ((*at == 'x' || *at == 'X') && validator->options.permit_hex_numbers)
        {
          ... go
        }
      else if (('0' <= *at && *at <= '7') && validator->options.permit_octal_numbers)
        {
          ... go
        }
      else if (*at == '.')
        {
          ...
        }
      else if (*at == 'e' || *at == 'E')
        {
          ...
        }
      else if (...)     // alpha/underscore no allowed
        {
          ..
        }
      else
        {
          // simply 0
          ...
        }
    
    case FLAT_VALUE_STATE_STRING_DIGIT_NOT_ALLOWED:
      {
        // if we just got a NUL, (\0) then subsequent digits are not allowed in JSON
        if ('0' <= *at && *at <= '9')
          {
            validator->error_code = JSON_VALIDATOR_ERROR_DIGIT_NOT_ALLOWED_AFTER_NUL;
            return SCAN_ERROR;
          }
        else
          FLAT_VALUE_GOTO_STATE(STRING);
      }
    case FLAT_VALUE_STATE_IN_DIGITS:
      ...
    case FLAT_VALUE_STATE_IN_OCTAL:
      ...
    case FLAT_VALUE_STATE_IN_HEX:
      ...
    case FLAT_VALUE_STATE_POST_E:
      ...
    case FLAT_VALUE_STATE_POST_E_PM_OR_DIGITS:
      ...
    case FLAT_VALUE_STATE_POST_E_DIGITS:
      ...
    case FLAT_VALUE_STATE_GOT_LEADING_DECIMAL_POINT:
      ...

    case FLAT_VALUE_STATE_IN_NULL:
      ...
    case FLAT_VALUE_STATE_IN_TRUE:
      ...
    case FLAT_VALUE_STATE_IN_FALSE:
      ...
    }

#undef FLAT_VALUE_GOTO_STATE
}

JSON_VALIDATOR_FUNC
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

#define IS_SPACE(c)  ((c)=='\t' || (c)=='\r' || (c)==' ' || (c)=='\n')

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
        switch (validator->options.format)                            \
          {                                                           \
          case JSON_VALIDATOR_ENCAPSULATION_ARRAY:                    \
            SET_STATE(INTERIM_IN_ARRAY_EXPECTING_COMMA);              \
          case JSON_VALIDATOR_ENCAPSULATION_LINE_BY_LINE:             \
            SET_STATE(INTERIM_EXPECTING_EOL);                         \
          case JSON_VALIDATOR_ENCAPSULATION_WHITESPACE_SEP:           \
            SET_STATE(INTERIM);                                       \
          }                                                           \
        assert(0);                                                    \
        RETURN_ERROR(INTERNAL);                                       \
      }                                                               \
    else if (validator->stack[validator->stack_depth].is_object)      \
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
                    goto case_INTERIM_GOT_COMMA;
                  }
              }
            else if (is_flat_value_char (*at))
              {
                if (!validator->permit_flat_values)
                  RETURN_ERROR(FLAT_VALUE_NOT_ALLOWED);
                validator->value_state = flat_value_char_value_state (*at);
                at++;
                SET_STATE(IN_FLAT_VALUE);
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
                SET_STATE (EXPECTING_EOF);
              }
            else if (*at == '{')
              {
                PUSH_OBJECT();
                at++;
                SET_STATE (IN_OBJECT_INITIAL);
              }
            else if (is_flat_value_char (validator, *at))
              {
                if (!validator->options.permit_flat_values)
                  {
                    ...
                  }
                validator->value_state = initial_flat_value_state (*at);
                at++;
                SET_STATE (INTERIM_IN_ARRAY_VALUE);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHARACTER);
              }
            break;

          CASE(INTERIM_IN_ARRAY_GOT_COMMA):
            if (*at == ']')
              {
                if (!validator->options.permit_trailing_commas)
                  RETURN_ERROR(UNEXPECTED_CHARACTER);
                POP();
              }
            else if (is_flat_value_char (*at))
              {
                ...
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
            else if (!validator->options.permit_multiple_commas)
              RETURN_ERROR(UNEXPECTED_CHARACTER);
            else
              RETURN_ERROR(UNEXPECTED_CHARACTER);
            break;

          // non-structured 
          CASE(INTERIM_IN_ARRAY_VALUE):
            {
            unsigned fv_used = 0;
            switch (flat_value_scan (validator, end-at, at, &fv_used))
              {
              case SCAN_END:
                at += fv_used;
                SET_STATE(IN_ARRAY_VALUE_EXPECTING_COMMA);

              case SCAN_ERROR:
                at += fv_used;
                RETURN_ERROR_CODE();

              case SCAN_IN_VALUE:
                at = end;
                goto at_end;
              }
              assert(0);
            }

          CASE(INTERIM_IN_ARRAY_VALUE_EXPECTING_COMMA):
            SKIP_WS();
            if (*at == ',')
              {
                at++;
                SET_STATE(INTERIM_IN_ARRAY_GOT_COMMA);
              }
            else if (*at == ']')
              {
                at++;
                SET_STATE(EXPECTING_EOF);
              }
            else
              {
                ...
              }
            break;

          CASE(IN_OBJECT):
            SKIP_WS();
            if (*at == '"')
              {
                at++;
                validator->string_state = VALIDATOR_STRING_STATE_NORMAL;
                SET_STATE (IN_OBJECT_FIELDNAME);
              }
            else if (validator->options.permit_bare_fieldnames
                &&  (IS_ASCII_ALPHA (*at) || *at == '_'))
              {
                at++;
                SET_STATE (IN_OBJECT_BARE_FIELDNAME);
              }

          CASE(IN_OBJECT_FIELDNAME):
            switch (scan_value (validator, &at, end))
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
                RETURN_ERROR(UNEXPECTED_CHARACTER);
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
            else if (is_flat_value_char (*at))
              {
                validator->value_state = flat_value_char_value_state (*at);
                at++;
                SET_STATE(IN_OBJECT_VALUE);
              }
            else
              {
                RETURN_ERROR(UNEXPECTED_CHARACTER);
              }

            // this state is only used for non-structured values; otherwise, we use the stack
          CASE(IN_OBJECT_VALUE):
            ...
            switch (scan_value (validator, &at, end))
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
                PUSH_OBJECT();
                SET_STATE(IN_OBJECT);
              }
            else if (*at == '[')
              {
                ...
                PUSH_ARRAY();
                SET_STATE(IN_ARRAY);
              }
            else if (*at == ']')
              {
                POP();
              }
            else if (*at == ',' && validator->options.permit_trailing_commas)
              SET_STATE(IN_ARRAY_GOT_COMMA);
            else if (is_flat_value_char (*at))
              {
                validator->value_state = flat_value_char_value_state (*at);
                at++;
                SET_STATE(IN_ARRAY_VALUE);
              }
            else 
              {
                RETURN_ERROR(UNEXPECTED_CHARACTER);
              }
            break;

          CASE(IN_ARRAY_VALUE):
            ...

          CASE(IN_ARRAY_GOT_COMMA):
            ...
        }
    }

at_end:
  *used = len;
  return (validator->state <= LAST_INTERIM_VALUE)
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
