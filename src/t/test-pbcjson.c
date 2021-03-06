#include "generated/test1.pb-c.h"
#include "../pbcrep.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define N_ELEMENTS(static_array) \
  (sizeof(static_array)/sizeof(static_array[0]))

typedef void (*CheckMessageFunc)(const ProtobufCMessage *msg);

typedef struct Test {
  const char *json;
  unsigned n_messages;
  CheckMessageFunc *message_checks;
  void (*check_error)(const PBCREP_Error *error);
} Test;



typedef struct TestInfo {
  Test *test;
  unsigned expect_index;
  bool got_error;
} TestInfo;

#if 0
static bool
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
#endif

static void
test_stream_persons (Test *test, unsigned max_feed)
{
  PBCREP_Parser_JSONOptions json_options = PBCREP_PARSER_JSON_OPTIONS_INIT;
  TestInfo state = { test, 0, false };
  PBCREP_Parser *parser = pbcrep_parser_new_json (&foo__person__descriptor,
                                                  &json_options);
  unsigned test_json_len = strlen (test->json);
  unsigned amt_fed = 0;
  PBCREP_Error *error = NULL;
  while (amt_fed < test_json_len)
    {
      unsigned amt = MIN (test_json_len - amt_fed, max_feed);
      if (!pbcrep_parser_feed (parser, amt, (const uint8_t *) test->json + amt_fed, &error))
        {
          assert (error != NULL);
          assert (state.expect_index == test->n_messages);
          assert (test->check_error != NULL);
          test->check_error (error);
          pbcrep_parser_destroy (parser);
          return;
        }
      amt_fed += amt;
    }
  if (!pbcrep_parser_end_feed (parser, &error))
    {
      assert (error != NULL);
      assert(state.expect_index == test->n_messages);
      assert (test->check_error != NULL);
      test->check_error (error);
      return;
    }
  pbcrep_parser_destroy (parser);
  assert (test->check_error == NULL);
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
static CheckMessageFunc basic_json__message_checks[1] = {
  basic_json__validate0
};
static Test basic_json__test = {
  basic_json__str,
  N_ELEMENTS(basic_json__message_checks),
  basic_json__message_checks,
  NULL
};


/*
perl -e '$x = 1; for ($i=0;$i<1000;$i++) { print "$x,"; $x *= 33; $x %= 10000000; print "\n" if ($i % 10 == 0); }'
*/
#define LINESEP ""
static const char long_int_array__str[] = 
"{\"test_ints\":["
"1,33,1089,35937,1185921,9135393,1467969,8442977,8618241,4401953,5264449,"
"3726817,2984961,8503713,622529,543457,7934081,1824673,214209,7068897,3273601,"
"8028833,4951489,3399137,2171521,1660193,4786369,7950177,2355841,7742753,5510849,"
"1858017,1314561,3380513,1556929,1378657,5495681,1357473,4796609,8288097,3507201,"
"5737633,9341889,8282337,3317121,9464993,2344769,7377377,3453441,3963553,797249,"
"6309217,8204161,737313,4331329,2933857,6817281,4970273,4019009,2627297,6700801,"
"1126433,7172289,6685537,622721,549793,8143169,8724577,7911041,1064353,5123649,"
"9080417,9653761,8574113,2945729,7209057,7898881,663073,1881409,2086497,8854401,"
"2195233,2442689,608737,88321,2914593,6181569,3991777,1728641,7045153,2490049,"
"2171617,1663361,4890913,1400129,6204257,4740481,6435873,2383809,8665697,5968001,"
"6944033,9153089,2051937,7713921,4559393,459969,5178977,906241,9905953,6896449,"
"7582817,232961,7687713,3694529,1919457,3342081,288673,9526209,4364897,4041601,"
"3372833,1303489,3015137,9499521,3484193,4978369,4286177,1443841,7646753,2342849,"
"7314017,1362561,4964513,3828929,6354657,9703681,221473,7308609,1184097,9075201,"
"9481633,2893889,5498337,1445121,7688993,3736769,3313377,9341441,8267553,2829249,"
"3365217,1052161,4721313,5803329,1509857,9825281,4234273,9731009,1123297,7068801,"
"3270433,7924289,1501537,9550721,5173793,735169,4260577,599041,9768353,2355649,"
"7736417,5301761,4958113,3617729,9385057,9706881,327073,793409,6182497,4022401,"
"2739233,394689,3024737,9816321,3938593,9973569,9127777,1216641,149153,4922049,"
"2427617,111361,3674913,1272129,1980257,5348481,6499873,4495809,8361697,5936001,"
"5888033,4305089,2067937,8241921,1983393,5451969,9914977,7194241,7409953,4528449,"
"9438817,1480961,8871713,2766529,1295457,2750081,752673,4838209,9660897,8809601,"
"716833,3655489,631137,827521,7308193,1170369,8622177,4531841,9550753,5174849,"
"770017,5410561,8548513,2100929,9330657,7911681,1085473,5820609,2080097,8643201,"
"5225633,2445889,714337,3573121,7912993,1128769,7249377,9229441,4571553,861249,"
"8421217,7900161,705313,3275329,8085857,6833281,5498273,1443009,7619297,1436801,"
"7414433,4676289,4317537,2478721,1797793,9327169,7796577,7287041,472353,5587649,"
"4392417,4949761,3342113,289729,9561057,5514881,1991073,5705409,8278497,3190401,"
"5283233,4346689,3440737,3544321,6962593,9765569,2263777,4704641,5253153,3354049,"
"683617,2559361,4458913,7144129,5756257,9956481,8563873,2607809,6057697,9904001,"
"6832033,5457089,83937,2769921,1407393,6443969,2650977,7482241,6913953,8160449,"
"9294817,6728961,2055713,7838529,8671457,6158081,3216673,6150209,2956897,7577601,"
"60833,2007489,6247137,6155521,3132193,3362369,958177,1619841,3454753,4006849,"
"2226017,3458561,4132513,6372929,306657,119681,3949473,332609,976097,2211201,"
"2969633,7997889,3930337,9701121,136993,4520769,9185377,3117441,2875553,4893249,"
"1477217,8748161,8689313,6747329,2661857,7841281,8762273,9155009,2115297,9804801,"
"3558433,7428289,5133537,9406721,421793,3919169,9332577,7975041,3176353,4819649,"
"9048417,8597761,3726113,2961729,7737057,5322881,5655073,6617409,8374497,6358401,"
"9827233,4298689,1856737,1272321,1986593,5557569,3399777,2192641,2357153,7786049,"
"6939617,9007361,7242913,9016129,7532257,8564481,2627873,6719809,1753697,7872001,"
"9776033,2609089,6099937,1297921,2831393,3435969,3386977,1770241,8417953,7792449,"
"7150817,5976961,7239713,8910529,4047457,3566081,7680673,3462209,4252897,345601,"
"1404833,6359489,9863137,5483521,956193,1554369,1294177,2707841,9358753,8838849,"
"1682017,5506561,1716513,6644929,9282657,6327681,8813473,844609,7872097,9779201,"
"2713633,9549889,5146337,9829121,4360993,3912769,9121377,1005441,3179553,4925249,"
"2533217,3596161,8673313,6219329,5237857,2849281,4026273,2867009,4611297,2172801,"
"1702433,6180289,3949537,334721,1045793,4511169,8868577,2663041,7880353,51649,"
"1704417,6245761,6110113,1633729,3913057,9130881,1319073,3529409,6470497,3526401,"
"6371233,250689,8272737,3000321,9010593,7349569,2535777,3680641,1461153,8218049,"
"1195617,9455361,2026913,6888129,7308257,1172481,8691873,6831809,5449697,9840001,"
"4720033,5761089,115937,3825921,6255393,6427969,2122977,58241,1921953,3424449,"
"3006817,9224961,4423713,5982529,7423457,4974081,4144673,6774209,3548897,7113601,"
"4748833,6711489,1479137,8811521,780193,5746369,9630177,7795841,7262753,9670849,"
"9138017,1554561,1300513,2916929,6258657,6535681,5677473,7356609,2768097,1347201,"
"4457633,7101889,4362337,3957121,584993,9304769,7057377,2893441,5483553,957249,"
"1589217,2444161,657313,1691329,5813857,1857281,1290273,2579009,5107297,8540801,"
"1846433,932289,765537,5262721,3669793,1103169,6404577,1351041,4584353,1283649,"
"2360417,7893761,494113,6305729,8089057,6938881,8983073,6441409,2566497,4694401,"
"4915233,2202689,2688737,8728321,8034593,5141569,9671777,9168641,2565153,4650049,"
"3451617,3903361,8810913,760129,5084257,7780481,6755873,2943809,7145697,5808001,"
"1664033,4913089,2131937,353921,1679393,5419969,8858977,2346241,7425953,5056449,"
"6862817,6472961,3607713,9054529,8799457,382081,2608673,6086209,844897,7881601,"
"92833,3063489,1095137,6139521,2604193,5938369,5966177,6883841,7166753,6502849,"
"4594017,1602561,2884513,5188929,1234657,743681,4541473,9868609,5664097,6915201,"
"8201633,653889,1578337,2085121,8808993,696769,2993377,8781441,9787553,2989249,"
"8645217,5292161,4641313,3163329,4389857,4865281,554273,8291009,3603297,8908801,"
"3990433,1684289,5581537,4190721,8293793,3695169,1940577,4039041,3288353,8515649,"
"1016417,3541761,6878113,6977729,265057,8746881,8647073,5353409,6662497,9862401,"
"5459233,154689,5104737,8456321,9058593,8933569,4807777,8656641,5669153,7082049,"
"3707617,2351361,7594913,632129,860257,8388481,6819873,5055809,6841697,5776001,"
"608033,65089,2147937,881921,9103393,411969,3594977,8634241,4929953,2688449,"
"8718817,7720961,4791713,8126529,8175457,9790081,3072673,1398209,6140897,2649601,"
"7436833,5415489,8711137,7467521,6428193,2130369,302177,9971841,9070753,9334849,"
"8050017,5650561,6468513,3460929,4210657,8951681,5405473,8380609,6560097,6483201,"
"3945633,205889,6794337,4213121,9032993,8088769,6929377,8669441,6091553,1021249,"
"3701217,2140161,625313,635329,965857,1873281,1818273,3009,99297,3276801,"
"8134433,8436289,8397537,7118721,4917793,2287169,5476577,727041,3992353,1747649,"
"7672417,3189761,5262113,3649729,441057,4554881,311073,265409,8758497,9030401,"
"8003233,4106689,5520737,2184321,2082593,8725569,7943777,2144641,773153,5514049,"
"1963617,4799361,8378913,6504129,4636257,2996481,8883873,3167809,4537697,9744001,"
"1552033,1217089,163937,5409921,8527393,1403969,6330977,8922241,4433953,6320449,"
"8574817,2968961,7975713,3198529,5551457,3198081,5536673,2710209,9436897,1417601,"
"6780833,3767489,4327137,2795521,2252193,4322369,2638177,7059841,2974753,8166849,"
"9506017,3698561,2052513,7732929,5186657,1159681,8269473,2892609,5456097,51201,"
"1689633,5757889,10337,341121,1256993,1480769,8865377,2557441,4395553,5053249,"
"6757217,2988161,8609313,4107329,5541857,2881281,5082273,7715009,4595297,1644801,"
"4278433,1188289,9213537,4046721,3541793,6879169,7012577,1415041,6696353,979649,"
"2328417,6837761,5646113,6321729,8617057,4362881,3975073,1177409,8854497,2198401,"
"2547233,4058689,3936737,9912321,7106593,4517569,9079777,9632641,7877153,9946049,"
"8219617,1247361,1162913,8376129,6412257,1604481,2947873,7279809,233697,7712001,"
"4496033,8369089,6179937,3937921,9951393,8395969,7066977,3210241,5937953,5952449,"
"6430817,2216961,3159713,4270529,927457,606081,673,22209,732897,4185601,"
"8124833,8119489,7943137,2123521,76193,2514369,2974177,8147841,8878753,2998849,"
"8962017,5746561,9636513,8004929,4162657,7367681,3133473,3404609,2352097,7619201,"
"1433633,7309889,1226337,469121,5480993,872769,8801377,445441,4699553,5085249,"
"7813217,7836161,8593313,3579329,8117857,7889281,346273,1427009,7091297,4012801,"
"2422433,9940289,8029537,4974721,4165793,7471169,6548577,6103041,1400353,6211649,"
"4984417,4485761,8030113,4993729,4793057,8170881,9639073,8089409,6950497,9366401,"
"9091233,10689,352737,1640321,4130593,6309569,8215777,1120641,6981153,378049,"
"2475617,1695361,5946913,6248129,6188257,4212481,9011873,7391809,3929697"
"]}"
;
static void long_int_array__validate0(const ProtobufCMessage *msg)
{
  const Foo__Person *person = (const Foo__Person *) msg;
  assert(IS_PERSON(msg));
  assert(person->n_test_ints == 1000);
  int32_t v = 1;
  for (unsigned size_i = 0; size_i < 1000; size_i++)
    {
      assert(person->test_ints[size_i] == v);
      v *= 33;
      v %= 10000000;
    }
}
static CheckMessageFunc long_int_array__message_checks[1] = {
  long_int_array__validate0
};
static Test long_int_array__test = {
  long_int_array__str,
  N_ELEMENTS(long_int_array__message_checks),
  long_int_array__message_checks,
  NULL
};



static const char empty_object__str[] = "{}\n";
static void empty_object__validate0(const ProtobufCMessage *msg)
{
  const Foo__Person *person = (const Foo__Person *) msg;
  assert(IS_PERSON(msg));
  assert(person->n_test_ints == 0);
}
static CheckMessageFunc empty_object__message_checks[1] = {
  empty_object__validate0
};
static Test empty_object__test = {
  empty_object__str,
  N_ELEMENTS(empty_object__message_checks),
  empty_object__message_checks,
  NULL
};


#define DUMP_ERROR(error) \
  fprintf(stderr, "error->message=%s\nerror->code=%s\n", error->error_message, error->error_code_str)
static const char fuck__str[] = "fuck";
static void fuck__check_error(const PBCREP_Error *error)
{
  //DUMP_ERROR(error);
  // XXX: not as thought there's any spec.
  //      "Bad Character" would be just as good.
  assert(strcmp(error->error_code_str, "EXPECTED_STRUCTURED_VALUE") == 0);
}
static Test fuck__test = {
  fuck__str,
  0, NULL,
  fuck__check_error
};


static Test *all_tests[] = {
  &basic_json__test,
  &long_int_array__test,
  &empty_object__test,
  &fuck__test,
};

static unsigned test_sizes[] = {
 1,2,3,4,5,9,11,13,20,2000
};

static void
usage (const char *prog_name)
{
  fprintf(stderr,
    "usage: %s [-d]\n\n"
    "Run the pbcjson tests.\n"
    ,
    prog_name
  );
  exit(1);
}

int main(int argc, char **argv)
{
  bool debug = false;
  for (int i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--debug") == 0
       || strcmp (argv[i], "-d") == 0)
        debug = true;
      else
        usage (argv[0]);
    }
  if (debug)
    {
      pbcrep_setup_debug_allocator ();
    }
  for (unsigned test_i = 0; test_i < N_ELEMENTS(all_tests); test_i++)
    {
      fprintf (stderr, "Test %s: ", all_tests[test_i]->json);
      for (unsigned size_i = 0; size_i < N_ELEMENTS(test_sizes); size_i++)
        {
          fprintf (stderr, "[size=%u] ", (unsigned) test_sizes[size_i]);
          test_stream_persons (all_tests[test_i], test_sizes[size_i]);
        }
      fprintf (stderr, " done.\n");
    }
  return 0;
}
