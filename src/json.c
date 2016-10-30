#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> /* errno, ERANGE */
#include <math.h> /* HUGE_VAL */
#include "json.h"

#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISWHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

#define JSON_CONTEXT_STACK_SIZE 256
#define PUTC(ctx, c) \
    do { \
        char ch = (c); \
        json_context_push(ctx, &ch, 1); \
    } while (0)

#define json_parse_true(c, v) json_parse_literal(c, v, "true", JSON_TRUE)
#define json_parse_false(c, v) json_parse_literal(c, v, "false", JSON_FALSE)
#define json_parse_null(c, v) json_parse_literal(c, v, "null", JSON_NULL)

typedef struct {
    const char *json;
    char *stack;
    size_t size;
    size_t top;
} json_context;

static void json_context_init(json_context *c, const char *json)
{
    c->json = json;
    c->stack = NULL;
    c->size = c->top = 0;
}

static void json_context_push(json_context *c, const void *v, size_t size)
{
    assert(v && size >= 0);
    if (c->top + size > c->size) {
        if (c->size == 0)
            c->size = JSON_CONTEXT_STACK_SIZE;
        while (c->top + size > c->size)
            c->size += c->size >> 1;
        c->stack = (char *) realloc(c->stack, c->size);
    }
    memcpy(c->stack + c->top, v, size);
    c->top += size;
}

static void *json_context_pop(json_context *c, size_t size)
{
    assert(c->top >= size);
    c->top -= size;
    return c->stack + c->top;
}

static void json_context_free(json_context *c)
{
    assert(c);
    free(c->stack);
}

static void json_parse_whitespace(json_context *c)
{
    while (ISWHITESPACE(*c->json))
        c->json++;
}

static int json_parse_literal(json_context *c, json_value *v, const char *literal, int type)
{
    size_t len;

    assert(*c->json == *literal);
    len = strlen(literal);
    if (strncmp(c->json, literal, len))
        return JSON_PARSE_ERROR;
    c->json += len;
    v->type = type;
    return JSON_PARSE_OK;
}

static int json_parse_number(json_context *c, json_value *v)
{
    const char *p = c->json;

    assert(ISDIGIT(*p) || *p == '-');
    if (*p == '-')
        p++;
    if (ISDIGIT(*p)) {
        if (*p == '0')
            p++;
        else
            for (p++; ISDIGIT(*p); p++)
                ;
    } else
        return JSON_PARSE_ERROR;
    if (*p == '.') {
        p++;
        if (ISDIGIT(*p))
            for (p++; ISDIGIT(*p); p++)
                ;
        else
            return JSON_PARSE_ERROR;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-' || *p == '+')
            p++;
        if (ISDIGIT(*p))
            for (p++; ISDIGIT(*p); p++)
                ;
        else
            return JSON_PARSE_ERROR;
    }
    errno = 0;
    v->number = strtod(c->json, NULL);
    /* man strtod HUGE_VAL */
    if (errno = ERANGE && (v->number == HUGE_VAL || v->number == -HUGE_VAL))
        return JSON_PARSE_ERROR;
    v->type = JSON_NUMBER;
    c->json = p;
    return JSON_PARSE_OK;
}

static const char *json_parse_hex4(const char *p, unsigned *u)
{
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        *u <<= 4;
        if (ISDIGIT(p[i]))
            *u |= p[i] - '0';
        else if (p[i] >= 'a' && p[i] <= 'f')
            *u |= p[i] - 'a' + 10;
        else if (p[i] >= 'A' && p[i] <= 'F')
            *u |= p[i] - 'A' + 10;
        else
            return NULL;
    }
    return p + 4;
}

static void json_encode_utf8(json_context *c, unsigned u)
{
    if (u <= 0x007F)
        PUTC(c, u & 0xFF);
    else if (u <= 0x07FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0x1F));
        PUTC(c, 0x80 | (u & 0x3F));
    } else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0x0F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    } else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0x07));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
}

static char *json_generate_string(json_context *c, size_t *len)
{
    size_t head = c->top;
    const char *p = c->json;
    unsigned hex;
    char *s;

    assert(*p == '\"');
    for (;;) {
        switch (*++p) {
        case '\"':
            c->json = ++p;
            *len = c->top - head;
            /* Bug: '\0'
            return strndup(json_context_pop(c, *len), *len);
             */
            s = (char *) malloc(*len + 1);
            memcpy(s, json_context_pop(c, *len), *len);
            s[*len] = '\0';
            return s;

        case '\\':
            switch (*++p) {
            case '\"':
            case '\\':
            case '/':
                PUTC(c, *p);
                break;

            case 'b':
                PUTC(c, '\b');
                break;
            case 'f':
                PUTC(c, '\f');
                break;
            case 'n':
                PUTC(c, '\n');
                break;
            case 'r':
                PUTC(c, '\r');
                break;
            case 't':
                PUTC(c, '\t');
                break;

            case 'u':
                if (!(p = json_parse_hex4(++p, &hex))) {
                    c->top = head;
                    return NULL;
                }
                if (hex >= 0xD800 && hex <= 0xDBFF) {
                    unsigned u;
                    if (*p++ != '\\' || *p++ != 'u' || !(p = json_parse_hex4(p, &u)) || !(u >= 0xDC00 && u <= 0xDFFF)) {
                        c->top = head;
                        return NULL;
                    }
                    hex = 0x10000 + ((hex - 0xD800) << 10) + (u - 0xDC00);
                }
                json_encode_utf8(c, hex);
                p--;
                break;

            default:
                c->top = head;
                return NULL;
            }
            break;

        default:
            if (*p < '\x20') {
                c->top = head;
                return NULL;
            }
            PUTC(c, *p);
        }
    }
}

static int json_parse_string(json_context *c, json_value *v)
{
    v->string = json_generate_string(c, &v->string_len);
    if (!v->string)
        return JSON_PARSE_ERROR;
    v->type = JSON_STRING;
    return JSON_PARSE_OK;
}

static int json_parse_value(json_context *c, json_value *v);

static int json_parse_array(json_context *c, json_value *v)
{
    size_t head = c->top;
    size_t size = 0;

    assert(*c->json == '[');
    c->json++;
    json_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        v->type = JSON_ARRAY;
        v->array_size = 0;
        v->array = NULL;
        return JSON_PARSE_OK;
    }
    for (;;) {
        json_value e;

        json_init(&e);
        if (json_parse_value(c, &e) == JSON_PARSE_ERROR)
            break;
        size++;
        json_context_push(c, &e, sizeof(json_value));
        json_parse_whitespace(c);
        if (*c->json == ']') {
            c->json++;
            v->type = JSON_ARRAY;
            v->array_size = size;
            size = sizeof(json_value) * size;
            v->array = (json_value *) malloc(size);
            memcpy(v->array, json_context_pop(c, size), size);
            return JSON_PARSE_OK;
        } else if (*c->json == ',') {
            c->json++;
            json_parse_whitespace(c);
        } else
            break;
    }
    while (c->top > head)
        json_free(json_context_pop(c, sizeof(json_value)));
    return JSON_PARSE_ERROR;
}

static int json_parse_object(json_context *c, json_value *v)
{
    size_t head = c->top;
    json_object **tail = &v->object;

    assert(*c->json == '{');
    v->type = JSON_OBJECT;
    v->object_size = 0;
    c->json++;
    json_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->object_size = 0;
        v->object = NULL;
        return JSON_PARSE_OK;
    }
    for (;;) {
        json_object o;

        if (*c->json != '\"' || (o.key = json_generate_string(c, &o.key_len)) == NULL)
            break;
        json_parse_whitespace(c);
        if (*c->json++ != ':') {
            free(o.key);
            break;
        }
        json_parse_whitespace(c);
        if (json_parse_value(c, &o.value) == JSON_PARSE_ERROR) {
            free(o.key);
            break;
        }
        v->object_size++;
        *tail = (json_object *) malloc(sizeof(json_object));
        memcpy(*tail, &o, sizeof(json_object));
        tail = &(*tail)->next;
        json_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            json_parse_whitespace(c);
        } else if (*c->json == '}') {
            c->json++;
            *tail = NULL;
            return JSON_PARSE_OK;
        } else
            break;
    }
    c->top = head;
    json_free(v);
    return JSON_PARSE_ERROR;
}

static int json_parse_value(json_context *c, json_value *v)
{
    switch (*c->json) {
    case '{':
        return json_parse_object(c, v);
    case '[':
        return json_parse_array(c, v);
    case '\"':
        return json_parse_string(c, v);
    case 't':
        return json_parse_true(c, v);
    case 'f':
        return json_parse_false(c, v);
    case 'n':
        return json_parse_null(c, v);
    default:
        return (ISDIGIT(*c->json) || *c->json == '-') ? json_parse_number(c, v) : JSON_PARSE_ERROR;
    }
}

void json_init(json_value *v)
{
    assert(v);
    v->type = JSON_NULL;
}

int json_parse(json_value *v, const char *json)
{
    int ret;
    json_context c;

    assert(v && json);
    json_init(v);
    json_context_init(&c, json);
    json_parse_whitespace(&c);
    if ((ret = json_parse_value(&c, v)) == JSON_PARSE_OK) {
        json_parse_whitespace(&c);
        if (*c.json != '\0') {
            json_free(v);
            ret = JSON_PARSE_ERROR;
        }
    }
    json_context_free(&c);
    return ret;
}

void json_free(json_value *v)
{
    size_t i;
    json_object *o;

    assert(v);
    switch (v->type) {
    case JSON_STRING:
        free(v->string);
        break;
    case JSON_ARRAY:
        for (i = 0; i < v->array_size; i++)
            json_free(&v->array[i]);
        free(v->array);
        break;
    case JSON_OBJECT:
        for (i = 0, o = v->object; i < v->object_size; i++) {
            json_object *next;
            free(o->key);
            json_free(&o->value);
            next = o->next;
            free(o);
            o = next;
        }
        break;
    default:
        break;
    }
    v->type = JSON_NULL;
}

int json_get_type(const json_value *v)
{
    assert(v);
    return v->type;
}

double json_get_number(const json_value *v)
{
    assert(v && v->type == JSON_NUMBER);
    return v->number;
}

char *json_get_string(const json_value *v)
{
    assert(v && v->type == JSON_STRING);
    return v->string;
}

size_t json_get_string_length(const json_value *v)
{
    assert(v && v->type == JSON_STRING);
    return v->string_len;
}

json_value *json_get_array_element(const json_value *v, size_t i)
{
    assert(v && v->type == JSON_ARRAY);
    return &v->array[i];
}

size_t json_get_array_size(const json_value *v)
{
    assert(v && v->type == JSON_ARRAY);
    return v->array_size;
}

size_t json_get_object_size(const json_value *v)
{
    assert(v && v->type == JSON_OBJECT);
    return v->object_size;
}

#define ASSERT_VALID_OBJECT_INDEX(v, index) \
    do { \
        assert(v && v->type == JSON_OBJECT && index >= 0 && index < v->object_size); \
    } while (0)

#define NTH_OBJECT(o, v, index) \
    do { \
        o = v->object; \
        for (; index > 0; index--) \
            o = o->next; \
    } while (0)

const char *json_get_object_key(const json_value *v, size_t index)
{
    json_object *o;

    ASSERT_VALID_OBJECT_INDEX(v, index);
    NTH_OBJECT(o, v, index);
    return o->key;
}

size_t json_get_object_key_length(const json_value *v, size_t index)
{
    json_object *o;

    ASSERT_VALID_OBJECT_INDEX(v, index);
    NTH_OBJECT(o, v, index);
    return o->key_len;
}

json_value *json_get_object_value_index(const json_value *v, size_t index)
{
    json_object *o;

    ASSERT_VALID_OBJECT_INDEX(v, index);
    NTH_OBJECT(o, v, index);
    return &o->value;
}

json_value *json_get_object_value_n(const json_value *v, const char *key, size_t len)
{
    json_object *o;

    assert(v && v->type == JSON_OBJECT && key);
    for (o = v->object; o; o = o->next)
        if (len == o->key_len && !memcmp(key, o->key, len))
            return &o->value;
    return NULL;
}
