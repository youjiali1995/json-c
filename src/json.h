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
typedef struct json_object json_object;

struct json_value {
    union {
        /* object */
        struct {
            json_object *object;
            size_t object_size;
        };
        /* array */
        struct {
            json_value *array;
            size_t array_size;
        };
        /* string */
        struct {
            char *string;
            size_t string_len;
        };
        /* number */
        double number;
    };
    json_type type;
};

struct json_object {
    char *key;
    size_t key_len;
    json_value value;
    json_object *next;
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

size_t json_get_object_size(const json_value *v);

const char *json_get_object_key(const json_value *v, size_t index);

size_t json_get_object_key_length(const json_value *v, size_t index);

json_value *json_get_object_value_index(const json_value *v, size_t index);

#define json_get_object_value(v, key) json_get_object_value_n(v, key, sizeof(key) - 1)

json_value *json_get_object_value_n(const json_value *v, const char *key, size_t len);

#endif /* JSON_H__ */
