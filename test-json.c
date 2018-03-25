#include "json-cb-parser.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef  struct Test {
  const char *json;
  const char *expected_callbacks_encoded;
} Test;

typedef  struct TestInfo {
  Test *test;
  const char *expected_callbacks_at;
} TestInfo;

static bool
test_json_cb__start_object  (void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '{');
  t->expected_callbacks_at++;
  return true;
}

static bool
test_json_cb__end_object    (void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '}');
  t->expected_callbacks_at++;
  return true;
}

static bool
test_json_cb__start_array   (void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '[');
  t->expected_callbacks_at++;
  return true;
}

static bool
test_json_cb__end_array     (void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == ']');
  t->expected_callbacks_at++;
  return true;
}

static bool
test_json_cb__object_key    (unsigned key_length,
                             const char *key,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 'k');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  assert(n == key_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      memcmp (key, t->expected_callbacks_at, key_length);
      t->expected_callbacks_at += key_length;
    }
  assert (*t->expected_callbacks_at == ' ');
  t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__number_value  (unsigned number_length,
                             const char *number,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 'n');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  assert(n == number_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      memcmp (number, t->expected_callbacks_at, number_length);
      t->expected_callbacks_at += number_length;
    }
  if (*t->expected_callbacks_at == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__string_value  (unsigned string_length,
                             const char *string,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 's');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  assert(n == string_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      memcmp (string, t->expected_callbacks_at, string_length);
      t->expected_callbacks_at += string_length;
    }
  if (*t->expected_callbacks_at == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__boolean_value (int boolean_value,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == boolean_value ? 'T' : 'F');
  t->expected_callbacks_at++;
  return true;
}
static bool
test_json_cb__null_value    (void *callback_data)
{
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 'N');
  t->expected_callbacks_at++;
  return true;
}

static bool
test_json_cb__error         (const JSON_CallbackParser_ErrorInfo *error,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  (void) error;//TODO
  assert(t->expected_callbacks_at[0] == 'E');
  assert(t->expected_callbacks_at[1] == '{');
  t->expected_callbacks_at += 2;
  switch (*t->expected_callbacks_at)
    {
      case '}':
        t->expected_callbacks_at++;
        break;
      default:
        assert(0);  //TODO
    }
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return false;
}

static void
test_json_cb__destroy       (void *callback_data)
{
  (void) callback_data; /// TODO assert that no other callbacks occur
}

JSON_Callbacks callbacks = JSON_CALLBACKS_DEF(test_json_cb__, );
static void
run_test (Test *test,
          unsigned max_write,
         JSON_CallbackParser_Options *options)
{
  TestInfo info = { test, test->expected_callbacks_encoded };
  JSON_CallbackParser *parser = json_callback_parser_new (&callbacks, &info, options);
  const char *json_at = test->json;
  unsigned json_rem = strlen (test->json);
  while (json_rem > 0)
    {
      unsigned amt = max_write < json_rem ? max_write : json_rem;
      if (!json_callback_parser_feed(parser, amt, (const uint8_t *) json_at))
        {
          assert(0);
        }
      json_rem -= amt;
      json_at += amt;
    }
  assert(info.expected_callbacks_at[0] == 0);
}


int main(void)
{
  JSON_CallbackParser_Options options = JSON_CALLBACK_PARSER_OPTIONS_INIT;
  Test test = {
    "{\"a\": 42, \"b\": \"c\", \"d\":[1,3]}",
    "{k1=a n2=42 k1=b s1=c k1=d [n1=1 n1=3]}"
  };
  run_test (&test, 1, &options);
  run_test (&test, 2, &options);
  run_test (&test, 16, &options);
  run_test (&test, 1024, &options);

  return 0;
}

