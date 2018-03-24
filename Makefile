
test-json: json-cb-parser.c json-cb-parser.h test-json.c
	cc -W -Wall -g -o test-json json-cb-parser.c test-json.c
