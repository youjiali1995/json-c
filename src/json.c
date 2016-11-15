#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> /* errno, ERANGE */
#include <math.h> /* HUGE_VAL */
#include <stdarg.h>
#include <stdio.h>
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

/* ***************************************Context***********************************************
 * 'json_context' contains a 'json' string and a dynamic stack used to keep the trace of parsing and buffer temporary results.
 *   1). When parsing, 'json' is a pointer to the next char for parsing, and 'stack' is a buffer to store the temporary results for parsing string and array.
 *   2). When jsonifying, 'stack' is a buffer to store the temporary json string.
 *   3). When setting json_value, 'stack' is a buffer to store the elements of array;
 */
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

/* ********************************Parse******************************************* */
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
    if (errno == ERANGE && (v->number == HUGE_VAL || v->number == -HUGE_VAL))
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
    return p + 3;
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
                    if (*++p != '\\' || *++p != 'u' || !(p = json_parse_hex4(++p, &u)) || !(u >= 0xDC00 && u <= 0xDFFF)) {
                        c->top = head;
                        return NULL;
                    }
                    hex = 0x10000 + ((hex - 0xD800) << 10) + (u - 0xDC00);
                }
                json_encode_utf8(c, hex);
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

/* Recursive descent parser */
int json_parse(json_value *v, const char *json)
{
    int ret;
    json_context c;

    assert(v && json);
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

void json_init(json_value *v)
{
    assert(v);
    v->type = JSON_NULL;
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

/* *******************************Jsonify*********************************** */
static const char *json_decode_utf8_to_codepoint(const char *p, unsigned *hex)
{
    *hex = 0;
    if ((*p & 0x80) == 0)
        *hex = (unsigned) *p;
    else if ((*p & 0xE0) == 0xC0) {
        *hex |= (*p++ & 0x1F) << 6;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p & 0x3F);
    } else if ((*p & 0xF0) == 0xE0) {
        *hex |= (*p++ & 0x0F) << 12;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p & 0x3F);
    } else if ((*p & 0xF8) == 0xF0) {
        *hex |= (*p++ & 0x07) << 18;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p++ & 0x3F) << 12;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) != 0x80)
            return NULL;
        *hex |= (*p & 0x3F);
    } else
        return NULL;
    return p;
}

static void json_htos(json_context *c, unsigned hex)
{
    char s[4];
    size_t i;

    json_context_push(c, "\\u", 2);
    /* underflow if use "for (i = 3; i >= 0; i--)" */
    for (i = 4; i > 0; i--) {
        s[i - 1] = "0123456789ABCDEF"[hex & 0xF];
        hex >>= 4;
    }
    json_context_push(c, s, 4);
}

static void json_decode_utf8(json_context *c, unsigned codepoint)
{
    if (codepoint < 0x10000)
        json_htos(c, codepoint);
    else {
        unsigned low, high;
        codepoint -= 0x10000;
        high = 0xD800 + (codepoint >> 10);
        low = 0xDC00 + (codepoint & 0x3FF);
        json_htos(c, high);
        json_htos(c, low);
    }
}

static int json_jsonify_string(json_context *c, const json_value *v)
{
    size_t head = c->top;
    const char *p, *end;

    assert(v->type == JSON_STRING);
    PUTC(c, '\"');
    for (p = v->string, end = v->string + v->string_len; p < end; p++)
        switch (*p) {
        case '\b':
            json_context_push(c, "\\b", 2);
            break;
        case '\f':
            json_context_push(c, "\\f", 2);
            break;
        case '\n':
            json_context_push(c, "\\n", 2);
            break;
        case '\r':
            json_context_push(c, "\\r", 2);
            break;
        case '\t':
            json_context_push(c, "\\t", 2);
            break;
        case '\"':
            json_context_push(c, "\\\"", 2);
            break;
        case '/':
            json_context_push(c, "\\/", 2);
            break;
        case '\\':
            json_context_push(c, "\\\\", 2);
            break;
        default:
            if (*p & 0x80 || *p < '\x20') {
                unsigned codepoint;
                if (!(p = json_decode_utf8_to_codepoint(p, &codepoint)) || codepoint > 0x10FFFF) {
                    c->top = head;
                    return JSON_JSONIFY_ERROR;
                }
                json_decode_utf8(c, codepoint);
            } else
                PUTC(c, *p);
            break;
        }
    PUTC(c, '\"');
    return JSON_JSONIFY_OK;
}

static int json_jsonify_number(json_context *c, const json_value *v)
{
    char d[50];
    int len;
    size_t head = c->top;

    assert(v->type == JSON_NUMBER);
    if ((len = snprintf(d, 50, "%.17g", v->number)) < 0) {
        c->top = head;
        return JSON_JSONIFY_ERROR;
    }
    json_context_push(c, d, len);
    return JSON_JSONIFY_OK;
}
static int json_jsonify_value(json_context *c, const json_value *v);

static int json_jsonify_array(json_context *c, const json_value *v)
{
    size_t head = c->top;
    size_t i;

    assert(v->type == JSON_ARRAY);
    PUTC(c, '[');
    for (i = 0; i < v->array_size; i++) {
        if (json_jsonify_value(c, v->array + i) == JSON_JSONIFY_ERROR) {
            c->top = head;
            return JSON_JSONIFY_ERROR;
        }
        if (i != v->array_size - 1)
            json_context_push(c, ", ", 2);
    }
    PUTC(c, ']');
    return JSON_JSONIFY_OK;
}

static int json_jsonify_object(json_context *c, const json_value *v)
{
    size_t head = c->top;
    json_object *p;

    assert(v->type == JSON_OBJECT);
    PUTC(c, '{');
    for (p = v->object; p; p = p->next) {
        PUTC(c, '\"');
        json_context_push(c, p->key, p->key_len);
        PUTC(c, '\"');
        json_context_push(c, ": ", 2);
        if (json_jsonify_value(c, &p->value) == JSON_JSONIFY_ERROR) {
            c->top = head;
            return JSON_JSONIFY_ERROR;
        }
        if (p->next)
            json_context_push(c, ", ", 2);
    }
    PUTC(c, '}');
    return JSON_JSONIFY_OK;
}

static int json_jsonify_value(json_context *c, const json_value *v)
{
    switch (v->type) {
    case JSON_NULL:
        json_context_push(c, "null", 4);
        break;
    case JSON_TRUE:
        json_context_push(c, "true", 4);
        break;
    case JSON_FALSE:
        json_context_push(c, "false", 5);
        break;
    case JSON_STRING:
        return json_jsonify_string(c, v);
    case JSON_NUMBER:
        return json_jsonify_number(c, v);
    case JSON_ARRAY:
        return json_jsonify_array(c, v);
    case JSON_OBJECT:
        return json_jsonify_object(c, v);
    default:
        return JSON_JSONIFY_ERROR;
    }
    return JSON_JSONIFY_OK;
}

char *json_jsonify(const json_value *v, size_t *len)
{
    json_context c;
    char *json;

    assert(v);
    json_context_init(&c, NULL);
    if (json_jsonify_value(&c, v) == JSON_JSONIFY_OK) {
        if (len)
            *len = c.top;
        json = (char *) malloc(c.top + 1);
        memcpy(json, c.stack, c.top);
        json[c.top] = '\0';
    } else {
        if (len)
            *len = 0;
        json = NULL;
    }
    json_context_free(&c);
    return json;
}

/* *********************************Access functions******************************** */
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

char *json_get_object_key(const json_value *v, size_t index)
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

void json_set_null(json_value *v)
{
    assert(v);
    v->type = JSON_NULL;
}

void json_set_true(json_value *v)
{
    assert(v);
    v->type = JSON_TRUE;
}

void json_set_false(json_value *v)
{
    assert(v);
    v->type = JSON_FALSE;
}

/* no validation checking */
void json_set_string(json_value *v, const char *string, size_t len)
{
    assert(v && string && len >= 0);
    v->type = JSON_STRING;
    v->string_len = len;
    v->string = (char *) malloc(len + 1);
    memcpy(v->string, string, len);
    v->string[len] = '\0';
}

void json_set_number(json_value *v, double number)
{
    assert(v);
    v->type = JSON_NUMBER;
    v->number = number;
}

static json_object *json_object_deepcopy(const json_object *o);

static json_value *json_value_deepcopy(const json_value *v)
{
    json_value *copy;
    size_t i;

    assert(v);
    copy = (json_value *) malloc(sizeof(json_value));
    switch (v->type) {
    case JSON_NULL:
    case JSON_TRUE:
    case JSON_FALSE:
        copy->type = v->type;
        break;
    case JSON_STRING:
        json_set_string(copy, v->string, v->string_len);
        break;
    case JSON_NUMBER:
        copy->type = JSON_NUMBER;
        copy->number = v->number;
        break;
    case JSON_ARRAY:
        copy->type = JSON_ARRAY;
        copy->array_size = v->array_size;
        copy->array = (json_value *) malloc(sizeof(json_value) * v->array_size);
        for (i = 0; i < v->array_size; i++) {
            json_value *e = json_value_deepcopy(v->array + i);
            memcpy(copy->array + i, e, sizeof(json_value));
            free(e);
        }
        break;
    case JSON_OBJECT:
    {
        json_object **o, *p;
        copy->type = JSON_OBJECT;
        copy->object_size = v->object_size;
        for (o = &copy->object, p = v->object; p; o = &(*o)->next, p = p->next)
            *o = json_object_deepcopy(p);
        *o = NULL;
        break;
    }
    default:
        free(copy);
        assert(0);
        return NULL;
    }
    return copy;
}

static json_object *json_object_deepcopy(const json_object *o)
{
    json_object *copy;
    json_value *value;

    assert(o);
    copy = (json_object *) malloc(sizeof(json_object));
    copy->key_len = o->key_len;
    copy->key = (char *) malloc(o->key_len + 1);
    memcpy(copy->key, o->key, o->key_len);
    copy->key[o->key_len] = '\0';
    value = json_value_deepcopy(&o->value);
    memcpy(&copy->value, value, sizeof(json_value));
    free(value);
    copy->next = o->next;
    return copy;
}

void json_set_array(json_value *v, int deepcopy, ...)
{
    va_list ap;
    json_context c;
    json_value *e;

    assert(v);
    json_context_init(&c, NULL);
    v->type = JSON_ARRAY;
    v->array_size = 0;
    va_start(ap, deepcopy);
    /* Deepcopy vs Shadowcopy */
    for (e = va_arg(ap, json_value *); e != NULL; e = va_arg(ap, json_value *)) {
        if (deepcopy)
            e = json_value_deepcopy(e);
        json_context_push(&c, e, sizeof(json_value));
        v->array_size++;
        if (deepcopy)
            free(e);
    }
    v->array = (json_value *) malloc(sizeof(json_value) * v->array_size);
    memcpy(v->array, json_context_pop(&c, c.top), c.top);
    json_context_free(&c);
    va_end(ap);
}

void json_object_append(json_value *v, int deepcopy, ...)
{
    va_list ap;
    json_object **p;
    char *key;

    assert(v);
    if (v->type != JSON_OBJECT) {
        v->type = JSON_OBJECT;
        v->object_size = 0;
        p = &v->object;
    } else
        for (p = &v->object; *p; p = &(*p)->next)
            ;
    va_start(ap, deepcopy);
    for (key = va_arg(ap, char *); key; key = va_arg(ap, char *)) {
        size_t key_len = va_arg(ap, size_t);
        json_value *value = va_arg(ap, json_value *);

        *p = (json_object *) malloc(sizeof(json_object));
        (*p)->key = (char *) malloc(key_len + 1);
        memcpy((*p)->key, key, key_len);
        (*p)->key_len = key_len;
        (*p)->key[key_len] = '\0';
        if (deepcopy) {
            value = json_value_deepcopy(value);
            memcpy(&(*p)->value, value, sizeof(json_value));
            free(value);
        } else
            memcpy(&(*p)->value, value, sizeof(json_value));
        p = &(*p)->next;
        v->object_size++;
    }
    *p = NULL;
    va_end(ap);
}
