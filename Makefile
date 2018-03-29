
test-json: json-cb-parser.c json-cb-parser.h test-json.c
	cc -W -Wall -g -o test-json json-cb-parser.c test-json.c

test-json-p: json-cb-parser.c json-cb-parser.h test-json.c
	gcc-7 -W -Wall -pg -g -o test-json-p json-cb-parser.c test-json.c

clean:
	rm -f test-json test-json-p
