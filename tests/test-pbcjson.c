#include "generated/test1.pb-c.h"
#include "pbc-parser-json.h"

typedef enum
{
  TEST_RESPONSE_CALLBACK_MESSAGE,
  TEST_RESPONSE_CALLBACK_ERROR,
} TestResponseCallbackType;

typedef struct {
  TestResponseCallbackType type;
  union {
    struct {
      void (*test)(const ProtobufCMessage *message, void *data);
      void *data;
    } message;
    struct {
      void (*callback)(const PBC_Parser_Error *error, void *data);
      void *data;
    } error;
  } info;
} TestResponseCallback;
#define TEST_RESPONSE_CALLBACK_MESSAGE(name, msgtype, code) \
  static void name##__testfunc (const ProtobufCMessage *pbc_protobufcmessage) \
  { \
    const msgtype *message = (const msgtype *) pbc_protobufcmessage; \
    code; \
  } \
  static TestResponsePiece name = { \
    .type: TEST_RESPONSE_PIECE_SIMPLE, \
    .info: { \
      .simple: (void (*)(const ProtobufCMessage*)) name##__testfunc  \
    } \
  }

static void test_simple (options)
{
  TEST_RESPONSE_CALLBACK_MESSAGE(...);
  Test test = {
    "{...}",
    n_callbacks, callbacks
  };
  ...
}

PIECE_SIMPLE(
  is_named_dave,
  Foo__Person,
  assert(strcmp(message.name, "dave") == 0)
);
PIECE_SIMPLE(
  is_email_dave_dave_com,
  Foo__Person,
  assert(strcmp(message.email, "dave@dave.com") == 0)
);
PIECE_SIMPLE(
  nphone_equals_0,
  Foo__Person,
  assert(message->n_phone == 0)
);
PIECE_SIMPLE(
  nphone_equals_1,
  Foo__Person,
  assert(message->n_phone == 1)
);

typedef struct Test {
  const char *json;
  unsigned n_callbacks;
  TestResponseCallback **callbacks;
} Test;

#define DEFINE_TEST(json_test, code) \

typedef struct TestInfo {
  Test *test;
  unsigned message_test_index;
} TestInfo;

static void
json_message_callback  (PBC_Parser             *parser,
                        const ProtobufCMessage *message,
                        void                   *callback_data)
{
  TestInfo *ti = callback_data;
  assert(ti->message_test_index < ti->test->n_message_tests);
  MessageTest *mt = ti->test->message_tests + ti->message_test_index;
  mt->test (message, mt->test_data);
}

static void
json_message_error_callback   (PBC_Parser             *parser,
                               const PBC_Parser_Error *error,
                               void                   *callback_data)
{
  ...
}

static PBC_ParserCallbacks callbacks =
{
  json_message_callback,
  json_message_error_callback,
  NULL
};

static void
test_stream_persons (Test *test, unsigned max_feed)
{
  PBC_Parser_JSONOptions json_options = PBC_PARSER_JSON_OPTIONS_INIT;
  TestState state = { test, 0 };
  PBC_Parser *parser = pbc_parser_new_json (&foo__person__descriptor,
                                            &json_options,
                                            &callbacks, &state);
  unsigned test_json_len = strlen (test->json_input);
  while (amt_fed < test_json_len)
    {
      unsigned amt = MIN (test_json_len - amt_fed, max_feed);
      if (!pbc_parser_feed (parser, amt, test->json_input + amt_fed))
        {
          ...
        }
      amt_fed += amt;
    }
  if (!pbc_parser_end_feed (parser))
    {
      ...
    }
  pbc_parser_destroy (parser);
}

