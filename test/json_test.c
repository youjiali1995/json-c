#include <stdlib.h>
#include <stdio.h>
#include "../src/json.h"

static int test_count = 0;
static int test_pass = 0;

#define ASSERT_EQ_BASE(equality, expect, actual, format) \
    do { \
        test_count++; \
        if (equality) \
            test_pass++; \
        else \
            fprintf(stderr, "%s, %d:\t" #expect " != (" #actual " == " format ")\n", __FILE__, __LINE__, actual); \
    } while (0)

#define ASSERT_EQ_INT(expect, actual) ASSERT_EQ_BASE((expect) == (actual), expect, actual, "%d")


#define TEST_PARSE_RESULT(result, type, json) \
    do { \
        json_value v; \
        json_init(&v); \
        ASSERT_EQ_INT(result, json_parse(&v, json)); \
        ASSERT_EQ_INT(type, json_get_type(&v)); \
    } while (0)

static void test_parse_true(void)
{
    TEST_PARSE_RESULT(JSON_PARSE_OK, JSON_TRUE, "true");
}

static void test_parse_false(void)
{
    TEST_PARSE_RESULT(JSON_PARSE_OK, JSON_FALSE, "false");
}

static void test_parse_null(void)
{
    TEST_PARSE_RESULT(JSON_PARSE_OK, JSON_NULL, "null");
}

static void test_parse_error(void)
{
    TEST_PARSE_RESULT(JSON_PARSE_ERROR, JSON_NULL, "tru");
    TEST_PARSE_RESULT(JSON_PARSE_ERROR, JSON_NULL, "FALSE");
    TEST_PARSE_RESULT(JSON_PARSE_ERROR, JSON_NULL, "nulll");
}

static void test_parse(void)
{
    test_parse_true();
    test_parse_false();
    test_parse_null();
    test_parse_error();
}

int main(void)
{
    test_parse();
    printf("=====================================================================================\n");
    printf("total:\t%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return 0;
}
