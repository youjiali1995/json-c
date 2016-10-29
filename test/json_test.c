#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/json.h"

static int test_count = 0;
static int test_pass = 0;

#define ASSERT_EQ_BASE(equality, expect, actual, format) \
    do { \
        test_count++; \
        if (equality) \
            test_pass++; \
        else \
            fprintf(stderr, "%s:%d:\t" #expect " != (" #actual " == " format ")\n", __FILE__, __LINE__, actual); \
    } while (0)

#define ASSERT_EQ_INT(expect, actual) ASSERT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define ASSERT_EQ_DOUBLE(expect, actual) ASSERT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define ASSERT_EQ_SIZE_T(expect, actual) ASSERT_EQ_BASE((expect) == (actual), expect, actual, "%zu")
/* Must not use strcmp to compare strings, because '\0' is a valid UNICODE character */
#define ASSERT_EQ_STRING(expect, actual, length) \
    ASSERT_EQ_BASE(sizeof(expect) - 1 == length && !memcmp(expect, actual, length), expect, actual, "%s")

#define TEST_PARSE_RESULT(result, type, json) \
    do { \
        json_value v; \
        json_init(&v); \
        ASSERT_EQ_INT(result, json_parse(&v, json)); \
        ASSERT_EQ_INT(type, json_get_type(&v)); \
        json_free(&v); \
    } while (0)

#define TEST_PARSE_ERROR(json) TEST_PARSE_RESULT(JSON_PARSE_ERROR, JSON_NULL, json)

#define TEST_PARSE_NUMBER(expect, json) \
    do { \
        json_value v; \
        json_init(&v); \
        ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, json)); \
        ASSERT_EQ_INT(JSON_NUMBER, json_get_type(&v)); \
        ASSERT_EQ_DOUBLE(expect, json_get_number(&v)); \
        json_free(&v); \
    } while (0)

#define TEST_PARSE_STRING(expect, json) \
    do { \
        json_value v; \
        json_init(&v); \
        ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, json)); \
        ASSERT_EQ_INT(JSON_STRING, json_get_type(&v)); \
        ASSERT_EQ_STRING(expect, json_get_string(&v), json_get_string_length(&v)); \
        json_free(&v); \
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

static void test_parse_number(void)
{
    TEST_PARSE_NUMBER(0.0, "0");
    TEST_PARSE_NUMBER(0.0, "0.0");
    TEST_PARSE_NUMBER(0.0, "-0.0");
    TEST_PARSE_NUMBER(123.0, "123");
    TEST_PARSE_NUMBER(3.1415926, "3.1415926");
    TEST_PARSE_NUMBER(1e10, "1e10");
    TEST_PARSE_NUMBER(1E10, "1E10");
    TEST_PARSE_NUMBER(1E+10, "1E+10");
    TEST_PARSE_NUMBER(1E-10, "1E-10");
    TEST_PARSE_NUMBER(-1E+10, "-1E+10");
    TEST_PARSE_NUMBER(-1E-10, "-1E-10");
    TEST_PARSE_NUMBER(-1.234E+10, "-1.234E+10");
    TEST_PARSE_NUMBER(0.0, "1E-10000");
}

static void test_parse_string(void)
{
    TEST_PARSE_STRING("", "\"\"");
    TEST_PARSE_STRING("hello, world", "\"hello, world\"");
    TEST_PARSE_STRING("hello\0world", "\"hello\\u0000world\"");
    TEST_PARSE_STRING("\t", "\"\\t\"");
    TEST_PARSE_STRING("\\", "\"\\\\\"");
    TEST_PARSE_STRING("/", "\"\\/\"");
    TEST_PARSE_STRING("\b", "\"\\b\"");
    TEST_PARSE_STRING("\f", "\"\\f\"");
    TEST_PARSE_STRING("\n", "\"\\n\"");
    TEST_PARSE_STRING("\r", "\"\\r\"");
    TEST_PARSE_STRING("\t", "\"\\t\"");
    TEST_PARSE_STRING("\"", "\"\\\"\"");
    /* UTF-8 */
    TEST_PARSE_STRING("\x24", "\"\\u0024\"");
    TEST_PARSE_STRING("\xC2\xA2", "\"\\u00A2\"");
    TEST_PARSE_STRING("\xE2\x82\xAC", "\"\\u20AC\"");
    TEST_PARSE_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");
    TEST_PARSE_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");
}

static void test_parse_array(void)
{
    json_value v;

    json_init(&v);
    ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, "[]"));
    ASSERT_EQ_INT(JSON_ARRAY, json_get_type(&v));
    ASSERT_EQ_SIZE_T(0, json_get_array_size(&v));
    json_free(&v);

    json_init(&v);
    ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, "[true]"));
    ASSERT_EQ_INT(JSON_TRUE, json_get_type(json_get_array_element(&v, 0)));
    ASSERT_EQ_SIZE_T(1, json_get_array_size(&v));
    json_free(&v);

    json_init(&v);
    ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, "[0, \"hello\", true, false, null, [1]]"));
    ASSERT_EQ_INT(JSON_ARRAY, json_get_type(&v));
    ASSERT_EQ_SIZE_T(6, json_get_array_size(&v));
    ASSERT_EQ_DOUBLE(0.0, json_get_number(json_get_array_element(&v, 0)));
    ASSERT_EQ_STRING("hello", json_get_string(json_get_array_element(&v, 1)), json_get_string_length(json_get_array_element(&v, 1)));
    ASSERT_EQ_INT(JSON_TRUE, json_get_type(json_get_array_element(&v, 2)));
    ASSERT_EQ_INT(JSON_FALSE, json_get_type(json_get_array_element(&v, 3)));
    ASSERT_EQ_INT(JSON_NULL, json_get_type(json_get_array_element(&v, 4)));
    ASSERT_EQ_INT(JSON_ARRAY, json_get_type(json_get_array_element(&v, 5)));
    ASSERT_EQ_SIZE_T(1, json_get_array_size(json_get_array_element(&v, 5)));
    ASSERT_EQ_DOUBLE(1.0, json_get_number(json_get_array_element(json_get_array_element(&v, 5), 0)));
    json_free(&v);
}

static void test_parse_error(void)
{
    /* Literal */
    TEST_PARSE_ERROR("");
    TEST_PARSE_ERROR("tru");
    TEST_PARSE_ERROR("FALSE");
    TEST_PARSE_ERROR("nulll");
    /* Number */
    TEST_PARSE_ERROR("+0");
    TEST_PARSE_ERROR("-");
    TEST_PARSE_ERROR("0123");
    TEST_PARSE_ERROR("0.");
    TEST_PARSE_ERROR(".123");
    TEST_PARSE_ERROR("1E");
    TEST_PARSE_ERROR("INF");
    TEST_PARSE_ERROR("NAN");
    TEST_PARSE_ERROR("0xFF");
    /* String */
    TEST_PARSE_ERROR("\"");
    TEST_PARSE_ERROR("\"abc\"\"");
    TEST_PARSE_ERROR("\"abc\\\"");
    TEST_PARSE_ERROR("\"\\v\"");
    TEST_PARSE_ERROR("\"\\0\"");
    TEST_PARSE_ERROR("\"\\x65\"");
    TEST_PARSE_ERROR("\"\0\"");
    TEST_PARSE_ERROR("\"\x1F\"");
    TEST_PARSE_ERROR("\"\\u\"");
    TEST_PARSE_ERROR("\"\\u0\"");
    TEST_PARSE_ERROR("\"\\u00\"");
    TEST_PARSE_ERROR("\"\\u000\"");
    TEST_PARSE_ERROR("\"\\u000G\"");
    TEST_PARSE_ERROR("\"\\U0000\"");
    TEST_PARSE_ERROR("\"\\uD800\"");
    TEST_PARSE_ERROR("\"\\uD8FF\"");
    TEST_PARSE_ERROR("\"\\uD800\\uDBFF\"");
    TEST_PARSE_ERROR("\"\\uD800\\uE000\"");
    TEST_PARSE_ERROR("[");
    TEST_PARSE_ERROR("]");
    TEST_PARSE_ERROR("[1,]");
    TEST_PARSE_ERROR("[,]");
    TEST_PARSE_ERROR("[1 2]");
}

static void test_free(void)
{
    json_value v;
    json_init(&v);
    ASSERT_EQ_INT(JSON_PARSE_OK, json_parse(&v, "\"hello\""));
    ASSERT_EQ_INT(JSON_STRING, json_get_type(&v));
    json_free(&v);
    ASSERT_EQ_INT(JSON_NULL, json_get_type(&v));
}

static void test(void)
{
    test_parse_true();
    test_parse_false();
    test_parse_null();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_error();
    test_free();
}

int main(void)
{
    test();
    printf("=====================================================================================\n");
    printf("total:\t%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return 0;
}
