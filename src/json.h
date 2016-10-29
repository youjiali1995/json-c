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
    union {
        /* object */
        struct {
            json_value *name;
            json_value *item;
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
    json_type type;
};

enum {
    JSON_PARSE_OK,
    JSON_PARSE_ERROR
};

void json_init(json_value *v);

int json_parse(json_value *v, const char *json);

void json_free(json_value *v);

int json_get_type(const json_value *v);

double json_get_number(const json_value *v);

char *json_get_string(const json_value *v);

size_t json_get_string_length(const json_value *v);

json_value *json_get_array_element(const json_value *v, size_t i);

size_t json_get_array_size(const json_value *v);

json_value *json_get_object_item(const json_value *v, const char *name);

#endif /* JSON_H__ */
