#ifndef JSON_H__
#define JSON_H__

#include <stddef.h> /* size_t */
#include <assert.h> /* assert */

typedef enum json_type {
    JSON_STRING,
    JSON_NUMBER,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL,
} json_type;

typedef struct json_value json_value;

struct json_value {
    json_type type;
    union {
        /* object */
        struct {
            char *name;
            json_value *object;
            json_value *next;
        };
        /* array */
        struct {
            size_t size;
            json_value *array;
        };
        /* string */
        struct {
            size_t len;
            char *string;
        };
        /* number */
        double number;
    };
};

enum {
    JSON_PARSE_OK,
    JSON_PARSE_ERROR
};

void json_init(json_value *v);

int json_parse(json_value *v, const char *json);

void json_free(json_value *v);

int json_get_type(json_value *v);

double json_get_number(json_value *v);

char *json_get_string(json_value *v);

size_t json_get_string_length(json_value *v);

#endif /* JSON_H__ */
