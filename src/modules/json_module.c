#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_module.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"
#include "memory.h"

static void setNative(ObjModule *module, const char *name, NativeFn function)
{
    ObjString *key = copyString(name, (int)strlen(name));
    push(OBJ_VAL(key));
    ObjNative *native = newNative(function);
    push(OBJ_VAL(native));
    tableSet(&module->fields, key, OBJ_VAL(native));
    pop();
    pop();
}

typedef struct
{
    const char *src;
    int pos;
    int len;
} JsonParser;

static Value parseValue(JsonParser *p);

static void skipWhitespace(JsonParser *p)
{
    while (p->pos < p->len &&
           (p->src[p->pos] == ' ' ||
            p->src[p->pos] == '\n' ||
            p->src[p->pos] == '\r' ||
            p->src[p->pos] == '\t'))
        p->pos++;
}

static Value parseString(JsonParser *p)
{
    p->pos++; // skip opening "
    int start = p->pos;
    char buffer[4096];
    int bufLen = 0;

    while (p->pos < p->len && p->src[p->pos] != '"')
    {
        if (p->src[p->pos] == '\\' && p->pos + 1 < p->len)
        {
            p->pos++;
            switch (p->src[p->pos])
            {
            case '"':
                buffer[bufLen++] = '"';
                break;
            case '\\':
                buffer[bufLen++] = '\\';
                break;
            case '/':
                buffer[bufLen++] = '/';
                break;
            case 'n':
                buffer[bufLen++] = '\n';
                break;
            case 't':
                buffer[bufLen++] = '\t';
                break;
            case 'r':
                buffer[bufLen++] = '\r';
                break;
            default:
                buffer[bufLen++] = p->src[p->pos];
                break;
            }
        }
        else
        {
            buffer[bufLen++] = p->src[p->pos];
        }
        p->pos++;
    }
    p->pos++; // skip closing "
    return OBJ_VAL(copyString(buffer, bufLen));
}

static Value parseNumber(JsonParser *p)
{
    int start = p->pos;
    if (p->src[p->pos] == '-')
        p->pos++;
    while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
        p->pos++;
    if (p->pos < p->len && p->src[p->pos] == '.')
    {
        p->pos++;
        while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
            p->pos++;
    }
    if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E'))
    {
        p->pos++;
        if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-'))
            p->pos++;
        while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
            p->pos++;
    }
    char buf[64];
    int numLen = p->pos - start;
    memcpy(buf, p->src + start, numLen);
    buf[numLen] = '\0';
    return NUMBER_VAL(strtod(buf, NULL));
}

static Value parseObject(JsonParser *p)
{
    p->pos++; // skip {
    skipWhitespace(p);

    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *instance = newInstance(klass);
    push(OBJ_VAL(instance));

    if (p->pos < p->len && p->src[p->pos] == '}')
    {
        p->pos++;
        pop(); // instance
        pop(); // klass
        return OBJ_VAL(instance);
    }

    while (p->pos < p->len)
    {
        skipWhitespace(p);
        if (p->src[p->pos] != '"')
            break;

        // parse key
        Value keyVal = parseString(p);
        ObjString *key = AS_STRING(keyVal);
        push(keyVal);

        skipWhitespace(p);
        if (p->src[p->pos] == ':')
            p->pos++;
        skipWhitespace(p);

        // parse value
        Value val = parseValue(p);
        push(val);

        tableSet(&instance->fields, key, val);

        pop(); // val
        pop(); // key

        skipWhitespace(p);
        if (p->pos < p->len && p->src[p->pos] == ',')
            p->pos++;
        else
            break;
    }

    skipWhitespace(p);
    if (p->pos < p->len && p->src[p->pos] == '}')
        p->pos++;

    pop(); // instance
    pop(); // klass
    return OBJ_VAL(instance);
}

static Value parseArray(JsonParser *p)
{
    p->pos++; // skip [
    skipWhitespace(p);

    ObjArray *array = newArray();
    push(OBJ_VAL(array));

    if (p->pos < p->len && p->src[p->pos] == ']')
    {
        p->pos++;
        pop();
        return OBJ_VAL(array);
    }

    while (p->pos < p->len)
    {
        skipWhitespace(p);
        Value val = parseValue(p);
        push(val);

        if (array->count == array->capacity)
        {
            int oldCap = array->capacity;
            array->capacity = GROW_CAPACITY(oldCap);
            array->values = GROW_ARRAY(Value, array->values, oldCap, array->capacity);
        }
        array->values[array->count++] = val;
        pop();

        skipWhitespace(p);
        if (p->pos < p->len && p->src[p->pos] == ',')
            p->pos++;
        else
            break;
    }

    skipWhitespace(p);
    if (p->pos < p->len && p->src[p->pos] == ']')
        p->pos++;

    pop();
    return OBJ_VAL(array);
}

static Value parseValue(JsonParser *p)
{
    skipWhitespace(p);
    if (p->pos >= p->len)
        return NIL_VAL;

    char c = p->src[p->pos];

    if (c == '"')
        return parseString(p);
    if (c == '{')
        return parseObject(p);
    if (c == '[')
        return parseArray(p);
    if (c == '-' || (c >= '0' && c <= '9'))
        return parseNumber(p);
    if (strncmp(p->src + p->pos, "true", 4) == 0)
    {
        p->pos += 4;
        return BOOL_VAL(true);
    }
    if (strncmp(p->src + p->pos, "false", 5) == 0)
    {
        p->pos += 5;
        return BOOL_VAL(false);
    }
    if (strncmp(p->src + p->pos, "null", 4) == 0)
    {
        p->pos += 4;
        return NIL_VAL;
    }

    return NIL_VAL;
}

static void stringifyValue(Value val, char *buf, int *pos, int maxLen);

static void appendStr(char *buf, int *pos, int maxLen, const char *str, int len)
{
    if (*pos + len >= maxLen)
        return;
    memcpy(buf + *pos, str, len);
    *pos += len;
}

static void stringifyValue(Value val, char *buf, int *pos, int maxLen)
{
    if (IS_NIL(val))
    {
        appendStr(buf, pos, maxLen, "null", 4);
    }
    else if (IS_BOOL(val))
    {
        if (AS_BOOL(val))
            appendStr(buf, pos, maxLen, "true", 4);
        else
            appendStr(buf, pos, maxLen, "false", 5);
    }
    else if (IS_NUMBER(val))
    {
        char num[64];
        int len = snprintf(num, sizeof(num), "%g", AS_NUMBER(val));
        appendStr(buf, pos, maxLen, num, len);
    }
    else if (IS_STRING(val))
    {
        ObjString *str = AS_STRING(val);
        appendStr(buf, pos, maxLen, "\"", 1);
        for (int i = 0; i < str->length; i++)
        {
            char c = str->chars[i];
            if (c == '"')
                appendStr(buf, pos, maxLen, "\\\"", 2);
            else if (c == '\\')
                appendStr(buf, pos, maxLen, "\\\\", 2);
            else if (c == '\n')
                appendStr(buf, pos, maxLen, "\\n", 2);
            else if (c == '\t')
                appendStr(buf, pos, maxLen, "\\t", 2);
            else if (c == '\r')
                appendStr(buf, pos, maxLen, "\\r", 2);
            else
            {
                buf[(*pos)++] = c;
            }
        }
        appendStr(buf, pos, maxLen, "\"", 1);
    }
    else if (IS_ARRAY(val))
    {
        ObjArray *arr = AS_ARRAY(val);
        appendStr(buf, pos, maxLen, "[", 1);
        for (int i = 0; i < arr->count; i++)
        {
            stringifyValue(arr->values[i], buf, pos, maxLen);
            if (i < arr->count - 1)
                appendStr(buf, pos, maxLen, ",", 1);
        }
        appendStr(buf, pos, maxLen, "]", 1);
    }
    else if (IS_INSTANCE(val))
    {
        ObjInstance *instance = AS_INSTANCE(val);
        appendStr(buf, pos, maxLen, "{", 1);
        bool first = true;
        for (int i = 0; i <= instance->fields.capacity; i++)
        {
            Entry *entry = &instance->fields.entries[i];

            if (entry->key == NULL)
                continue;
            if (!first)
                appendStr(buf, pos, maxLen, ",", 1);
            first = false;
            appendStr(buf, pos, maxLen, "\"", 1);
            appendStr(buf, pos, maxLen, entry->key->chars, entry->key->length);
            appendStr(buf, pos, maxLen, "\":", 2);
            stringifyValue(entry->value, buf, pos, maxLen);
        }
        appendStr(buf, pos, maxLen, "}", 1);
    }
    else
    {
        appendStr(buf, pos, maxLen, "null", 4);
    }
}

static Value jsonParse(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);

    JsonParser parser;
    parser.src = str->chars;
    parser.pos = 0;
    parser.len = str->length;

    return parseValue(&parser);
}

static Value jsonStringify(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;

    char buf[65536];
    int pos = 0;
    stringifyValue(args[0], buf, &pos, sizeof(buf));
    buf[pos] = '\0';
    return OBJ_VAL(copyString(buf, pos));
}

static Value jsonKeys(int argCount, Value *args)
{
    if (argCount != 1 || !IS_INSTANCE(args[0]))
        return NIL_VAL;
    ObjInstance *instance = AS_INSTANCE(args[0]);
    ObjArray *result = newArray();
    push(OBJ_VAL(result));
    for (int i = 0; i <= instance->fields.capacity; i++)
    {
        Entry *entry = &instance->fields.entries[i];
        if (entry->key == NULL)
            continue;
        if (result->count == result->capacity)
        {
            int oldCap = result->capacity;
            result->capacity = GROW_CAPACITY(oldCap);
            result->values = GROW_ARRAY(Value, result->values, oldCap, result->capacity);
        }
        result->values[result->count++] = OBJ_VAL(entry->key);
    }
    pop();
    return OBJ_VAL(result);
}

ObjModule *initJsonModule(void)
{
    ObjString *name = copyString("json", 4);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "parse", jsonParse);
    setNative(module, "stringify", jsonStringify);
    setNative(module, "keys", jsonKeys);

    pop(); // module
    pop(); // name
    return module;
}
