#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISWHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

typedef struct {
    const char *json;
} json_context;

static void json_parse_whitespace(json_context *c)
{
    assert(c);
    while (ISWHITESPACE(*c->json))
        c->json++;
}

static int json_parse_object(json_context *c, json_value *v)
{
}

static int json_parse_array(json_context *c, json_value *v)
{
}

static int json_parse_string(json_context *c, json_value *v)
{
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

/*
static int json_parse_true(json_context *c, json_value *v)
{
    assert(*c->json == 't');
    if (strncmp(c->json, "true", 4))
        return JSON_PARSE_ERROR;
    c->json += 4;
    v->type = JSON_TRUE;
    return JSON_PARSE_OK;
}

static int json_parse_false(json_context *c, json_value *v)
{
    assert(*c->json == 'f');
    if (strncmp(c->json, "false", 5))
        return JSON_PARSE_ERROR;
    c->json += 5;
    v->type = JSON_FALSE;
    return JSON_PARSE_OK;
}

static int json_parse_null(json_context *c, json_value *v)
{
    assert(*c->json == 'n');
    if (strncmp(c->json, "null", 4))
        return JSON_PARSE_ERROR;
    c->json += 4;
    v->type = JSON_NULL;
    return JSON_PARSE_OK;
}
*/

static int json_parse_number(json_context *c, json_value *v)
{
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
        return json_parse_literal(c, v, "true", JSON_TRUE);
    case 'f':
        return json_parse_literal(c, v, "false", JSON_FALSE);
    case 'n':
        return json_parse_literal(c, v, "null", JSON_NULL);
    default:
        return (ISDIGIT(*c->json) || *c->json == '-') ? json_parse_number(c, v) : JSON_PARSE_ERROR;
    }
}

int json_parse(json_value *v, const char *json)
{
    int ret;
    json_context c;

    assert(v);
    json_init(v);
    c.json = json;
    json_parse_whitespace(&c);
    if ((ret = json_parse_value(&c, v)) == JSON_PARSE_OK) {
        json_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = JSON_NULL;
            ret = JSON_PARSE_ERROR;
        }
    }
    return ret;
}

json_type json_get_type(const json_value *v)
{
    assert(v);
    return v->type;
}
