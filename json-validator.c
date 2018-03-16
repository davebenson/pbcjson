#include "json-validator.h"

typedef enum
{
  JSON_VALIDATOR_STATE_INTERIM,
  ...
} JSON_ValidatorState;


typedef struct JSON_Validator_StackNode {
  uint8_t is_object : 1;                        // otherwise, it's an array
} JSON_Validator_StackNode;

struct JSON_Validator {
  JSON_Validator_Options options;

  // NOTE: The stack doesn't include the outside array for ARRAY_OF_OBJECTS
  unsigned stack_depth;
  JSON_Validator_StackNode *stack_nodes;
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
  return validator;
}



JSON_ValidatorResult
json_validator_feed (JSON_Validator *validator,
                     size_t          len,
                     const uint8_t  *data,
                     size_t         *used)
{
  const uint8_t *end = data + len, *at = data;

#define SKIP_CHAR_TYPE(predicate)                                    \
  do {                                                               \
    while (at < end  &&  predicate(*at))                             \
      at++;                                                          \
    if (at == end)                                                   \
      goto at_end;                                                   \
  }while(0)

#define ISSPACE(c)  ((c)=='\t' || (c)=='\r' || (c)==' ' || (c)=='\n')

#define SKIP_WS()   SKIP_CHAR_TYPE(ISSPACE)

  while (at < end)
    {
      switch (validator->state)
        {
          case JSON_VALIDATOR_STATE_INTERIM:
            SKIP_WS();
            if (*at == '['
              && validator->options.format == JSON_VALIDATOR_FORMAT_ARRAY_OF_OBJECTS)
              {
                validator->state = JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY;
                at++;
                continue;
              }
            if (*at == '{')
              {
                if (validator->options.format == JSON_VALIDATOR_FORMAT_ARRAY_OF_OBJECTS)
                  return JSON_VALIDATOR_ERROR_EXPECTED_ARRAY_START;

                // push object marker onto stack
                ...
                validator->state = JSON_VALIDATOR_STATE_IN_OBJECT;
              }
            if (*at == ',')
              {
                if (validator->options.format == JSON_VALIDATOR_FORMAT_LAX_OBJECTS)
                  {
                    at++;
                    continue;
                  }
              }

            if ('0' <= *at && *at <= '9')
              {
                if (!validator->permit_bare_values)
                  RETURN_ERROR(JSON_VALIDATOR_ERROR_BARE_NUMBER_NOT_ALLOWED);
                validator->state = JSON_VALIDATOR_STATE_IN_BARE_VALUE;
                validator->value_state = VALUE_STATE_IN_INTEGER_PART;
                at++;
                continue;
              }
            if (*at == '"')
              {
                ...
              }
            validator->state = JSON_VALIDATOR_ERROR_EXPECTED_ARRAY_START
            break;

          case JSON_VALIDATOR_STATE_INTERIM_IN_ARRAY:
            ...

          case JSON_VALIDATOR_STATE_IN_OBJECT:
            ...

          case JSON_VALIDATOR_STATE_IN_OBJECT_FIELDNAME:
            ...

          case JSON_VALIDATOR_STATE_IN_OBJECT_AWAITING_COLON:
            ...
          case JSON_VALIDATOR_STATE_IN_OBJECT_VALUE_SPACE:
            ...

            // this state is only used for non-structured values; otherwise, we use the stack
          case JSON_VALIDATOR_STATE_IN_OBJECT_VALUE:
            ...

          case JSON_VALIDATOR_STATE_IN_ARRAY:
            ...
        }
    }

at_end:
  -
}
