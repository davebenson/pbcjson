#include "generated/test1.pb-c.h"
#include "../pbcrep.h"
#include <string.h>

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define N_ELEMENTS(static_array) \
  (sizeof(static_array)/sizeof(static_array[0]))

typedef struct Expect {
  void (*callback_message)(const ProtobufCMessage *msg);
  void (*callback_error)(const PBCREP_Error *error);
} Expect;

typedef struct Test {
  const char *json;
  unsigned n_expects;
  Expect *expects;
} Test;



typedef struct TestInfo {
  Test *test;
  unsigned expect_index;
  bool got_error;
} TestInfo;

static void
json_message_callback  (PBCREP_Parser             *parser,
                        const ProtobufCMessage *message,
                        void                   *callback_data)
{
  assert(pbcrep_parser_is_json (parser));

  TestInfo *ti = callback_data;
  assert(ti->expect_index < ti->test->n_expects);
  Expect *mt = ti->test->expects + ti->expect_index;
  assert(mt->callback_message != NULL);
  mt->callback_message (message);
  ti->expect_index++;
}

static void
json_message_error_callback   (PBCREP_Parser      *parser,
                               const PBCREP_Error *error,
                               void               *callback_data)
{
  assert(pbcrep_parser_is_json (parser));

  TestInfo *ti = callback_data;
  assert(ti->expect_index < ti->test->n_expects);
  Expect *mt = ti->test->expects + ti->expect_index;
  if (mt->callback_message != NULL) {
    fprintf(stderr, "got unexpected error in json_message_error_callback: %s:%u: %s\n", __FILE__, __LINE__, error->error_message);
    // fallthrough
  }
  assert(mt->callback_message == NULL);
  assert(mt->callback_error != NULL);
  mt->callback_error (error);
  ti->expect_index++;
  ti->got_error = true;
}

#define target_from_test_info(test_info) \
  ((PBCREP_ParserTarget) { \
    .message_callback = json_message_callback, \
    .error_callback = json_message_error_callback, \
    .callback_data = &(test_info), \
    .destroy = NULL \
  })

static void
test_stream_persons (Test *test, unsigned max_feed)
{
  PBCREP_Parser_JSONOptions json_options = PBCREP_PARSER_JSON_OPTIONS_INIT;
  TestInfo state = { test, 0, false };
  PBCREP_Parser *parser = pbcrep_parser_new_json (&foo__person__descriptor,
                                            &json_options,
                                            target_from_test_info(state));
  unsigned test_json_len = strlen (test->json);
  unsigned amt_fed = 0;
  while (amt_fed < test_json_len)
    {
      unsigned amt = MIN (test_json_len - amt_fed, max_feed);
      if (!pbcrep_parser_feed (parser, amt, (const uint8_t *) test->json + amt_fed))
        {
          assert(state.got_error);
          assert(state.expect_index == test->n_expects);
          pbcrep_parser_destroy (parser);
          return;
        }
      amt_fed += amt;
    }
  if (!pbcrep_parser_end_feed (parser))
    {
      assert(state.got_error);
      assert(state.expect_index == test->n_expects);
      return;
    }
  pbcrep_parser_destroy (parser);
}

#define IS_PERSON(msg) \
  (((ProtobufCMessage*)(msg))->descriptor == &foo__person__descriptor)
#define IS_PHONE_NUMBER(msg) \
  (((ProtobufCMessage*)(msg))->descriptor == &foo__person__phone_number__descriptor)

static const char basic_json__str[] = 
"{\"name\":\"daveb\","
  "\"id\":42,"
  "\"email\":\"dave@dave.com\","
  "\"phone\":[{"
    "\"number\":\"555-1111\","
    "\"type\":\"MOBILE\""
  "},"
  "{"
    "\"number\":\"555-1112\","
    "\"type\":\"WORK\""
  "}],"
  "\"test_ints\":[1,2,3]"
"}";
static void basic_json__validate0(const ProtobufCMessage *msg)
{
  const Foo__Person *person = (const Foo__Person *) msg;
  assert(IS_PERSON(msg));
  assert(strcmp (person->name, "daveb") == 0);
  assert(person->id == 42);
  assert(strcmp (person->email, "dave@dave.com") == 0);
  assert(person->n_phone == 2);
  assert(IS_PHONE_NUMBER(person->phone[0]));
  assert(strcmp (person->phone[0]->number, "555-1111") == 0);
  assert(person->phone[0]->has_type);
  assert(person->phone[0]->type == FOO__PERSON__PHONE_TYPE__MOBILE);
  assert(IS_PHONE_NUMBER(person->phone[1]));
  assert(strcmp (person->phone[1]->number, "555-1112") == 0);
  assert(person->phone[1]->has_type);
  assert(person->phone[1]->type == FOO__PERSON__PHONE_TYPE__WORK);
  assert(person->n_test_ints == 3);
  assert(person->test_ints[0] == 1);
  assert(person->test_ints[1] == 2);
  assert(person->test_ints[2] == 3);
}
static Expect basic_json__expects[1] = {
  { basic_json__validate0, NULL }
};
static Test basic_json__test = {
  basic_json__str,
  N_ELEMENTS(basic_json__expects),
  basic_json__expects
};

static Test *all_tests[] = {
  &basic_json__test
};

static unsigned test_sizes[] = {
 1,2,3,4,5,9,11,13,20,2000
};

int main()
{
  for (unsigned test_i = 0; test_i < N_ELEMENTS(all_tests); test_i++)
    for (unsigned size_i = 0; size_i < N_ELEMENTS(test_sizes); size_i++)
      test_stream_persons (all_tests[test_i], test_sizes[size_i]);
  return 0;
}
