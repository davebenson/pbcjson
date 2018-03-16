
typedef enum
{
  JSON_VALIDATOR_ENCAPSULATION_LINE_BY_LINE,
  JSON_VALIDATOR_ENCAPSULATION_ARRAY,
  JSON_VALIDATOR_ENCAPSULATION_WHITESPACE_SEP,
} JSON_Validator_Encapsulation;

struct JSON_Validator_Options {
  unsigned max_stack_depth;

  // These flags are chosen so that 0 is the strict/standard JSON interpretation.
  unsigned ignore_utf8_errors : 1;
  unsigned ignore_utf16_errors : 1;
  unsigned ignore_utf8_surrogate_pairs : 1;
  unsigned ignore_trailing_commas : 1;
  unsigned ignore_comments : 1;


  // These values describe the encapsulation of the JSON records
  unsigned permit_bare_values : 1;
  unsigned permit_array_values : 1;
  unsigned permit_toplevel_commas : 1;
  JSON_Validator_Encapsulation encapsulation;

  // Used for error information.
  unsigned start_line_number;
};


typedef enum
{
  JSON_VALIDATOR_RESULT_INTERIM,           // spacing etc in between records
  JSON_VALIDATOR_RESULT_IN_RECORD,         // record did not come to an end (*used==len)
  JSON_VALIDATOR_RESULT_RECORD_ENDED,      // spacing etc in between records
  JSON_VALIDATOR_RESULT_ERROR              // call json_validator_get_error_info()
} JSON_ValidatorResult;
  


typedef enum
{
  JSON_VALIDATOR_ERROR_OK,                      // used if you get_error_info() without an error

  JSON_VALIDATOR_ERROR_EXPECTED_DOUBLE_QUOTE,
  JSON_VALIDATOR_ERROR_EXPECTED_COLON,
  JSON_VALIDATOR_ERROR_EXPECTED_VALUE,
  JSON_VALIDATOR_ERROR_EXPECTED_COMMA_OR_RBRACKET,
  JSON_VALIDATOR_ERROR_EXPECTED_COMMA_OR_RBRACE,
  JSON_VALIDATOR_ERROR_EXTRA_COMMA,
  JSON_VALIDATOR_ERROR_BAD_NUMBER,

  // errors that can occur while validating a string.
  JSON_VALIDATOR_ERROR_UTF8_ILSEQ,
  JSON_VALIDATOR_ERROR_UTF8_SHORT,
  JSON_VALIDATOR_ERROR_UTF8_SURROGATE_PAIR_NOT_ALLOWED,
  JSON_VALIDATOR_ERROR_UTF16_BAD_SURROGATE_PAIR,
  JSON_VALIDATOR_ERROR_STRING_CONTROL_CHARACTER,
} JSON_ValidatorError;

typedef enum
{
  JSON_VALIDATOR_FUNDAMENTAL_ERROR_...,
} JSON_Validator_FundamentalError;

//---------------------------------------------------------------------
//                           Functions
//---------------------------------------------------------------------
size_t json_validator_options_get_memory_size (const JSON_Validator_Options *options);

// note thtat JSON_Validator allocates no further memory, so there
JSON_Validator * json_validator_init (void *memory,
                                      const JSON_Validator_Options *options);
 


JSON_ValidatorResult
json_validator_feed (JSON_Validator *validator,
                     size_t          len,
                     const uint8_t  *data,
                     size_t         *used);

JSON_ValidatorResult
json_validator_end_feed (JSON_Validator *validator);


JSON_ValidatorError
json_validator_get_error_info(JSON_Validator *validator);
