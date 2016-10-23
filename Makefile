json_test:
	gcc -o json_test test/json_test.c src/json.c

json_debug:
	gcc -o json_debug -g test/json_test.c src/json.c

clean:
	rm json_test
