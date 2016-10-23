#ifndef JSON_H__
#define JSON_H__

#include <stddef.h> /* size_t */

typedef enum {
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

#define json_init(v) ((v)->type = JSON_NULL)

int json_parse(json_value *v, const char *json);

json_type json_get_type(const json_value *v);

void json_free(json_value *v);


#endif /* JSON_H__ */
