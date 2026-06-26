#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "str_module.h"
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

static Value strUpper(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    char *buffer = (char *)malloc(str->length + 1);
    for (int i = 0; i < str->length; i++)
        buffer[i] = toupper((unsigned char)str->chars[i]);
    buffer[str->length] = '\0';
    ObjString *result = copyString(buffer, str->length);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strLower(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    char *buffer = (char *)malloc(str->length + 1);
    for (int i = 0; i < str->length; i++)
        buffer[i] = tolower((unsigned char)str->chars[i]);
    buffer[str->length] = '\0';
    ObjString *result = copyString(buffer, str->length);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strTrim(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *chars = AS_CSTRING(args[0]);
    int len = AS_STRING(args[0])->length;

    int start = 0;
    int end = len - 1;

    while (start <= end && isspace((unsigned char)chars[start]))
        start++;
    while (end >= start && isspace((unsigned char)chars[end]))
        end--;

    return OBJ_VAL(copyString(chars + start, end - start + 1));
}

static Value strTrimLeft(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *chars = AS_CSTRING(args[0]);
    int len = AS_STRING(args[0])->length;

    int start = 0;
    while (start < len && isspace((unsigned char)chars[start]))
        start++;

    return OBJ_VAL(copyString(chars + start, len - start));
}

static Value strTrimRight(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *chars = AS_CSTRING(args[0]);
    int len = AS_STRING(args[0])->length;

    int end = len - 1;
    while (end >= 0 && isspace((unsigned char)chars[end]))
        end--;

    return OBJ_VAL(copyString(chars, end + 1));
}

static Value strLen(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(AS_STRING(args[0])->length);
}

static Value strCharAt(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    int index = (int)AS_NUMBER(args[1]);
    if (index < 0 || index >= str->length)
        return NIL_VAL;
    return OBJ_VAL(copyString(&str->chars[index], 1));
}

static Value strIndexOf(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    const char *haystack = AS_CSTRING(args[0]);
    const char *needle = AS_CSTRING(args[1]);
    const char *found = strstr(haystack, needle);
    if (found == NULL)
        return NUMBER_VAL(-1);
    return NUMBER_VAL((double)(found - haystack));
}

static Value strContains(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    return BOOL_VAL(strstr(AS_CSTRING(args[0]), AS_CSTRING(args[1])) != NULL);
}

static Value strStartsWith(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    ObjString *prefix = AS_STRING(args[1]);
    if (prefix->length > str->length)
        return BOOL_VAL(false);
    return BOOL_VAL(memcmp(str->chars, prefix->chars, prefix->length) == 0);
}

static Value strEndsWith(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    ObjString *suffix = AS_STRING(args[1]);
    if (suffix->length > str->length)
        return BOOL_VAL(false);
    return BOOL_VAL(memcmp(str->chars + str->length - suffix->length, suffix->chars, suffix->length) == 0);
}

static Value strSlice(int argCount, Value *args)
{
    if (argCount != 3 || !IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int end = (int)AS_NUMBER(args[2]);
    if (start < 0 || end > str->length || start > end)
        return NIL_VAL;
    return OBJ_VAL(copyString(str->chars + start, end - start));
}

static Value strReplace(int argCount, Value *args)
{
    if (argCount != 3 || !IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2]))
        return NIL_VAL;
    const char *src = AS_CSTRING(args[0]);
    const char *needle = AS_CSTRING(args[1]);
    const char *replacement = AS_CSTRING(args[2]);
    int srcLen = AS_STRING(args[0])->length;
    int needleLen = AS_STRING(args[1])->length;
    int repLen = AS_STRING(args[2])->length;

    if (needleLen == 0)
        return args[0];

    // count occurrences
    int count = 0;
    const char *p = src;
    while ((p = strstr(p, needle)) != NULL)
    {
        count++;
        p += needleLen;
    }

    // build result
    int outLen = srcLen + count * (repLen - needleLen);
    char *buffer = (char *)malloc(outLen + 1);
    char *out = buffer;
    p = src;

    const char *found;
    while ((found = strstr(p, needle)) != NULL)
    {
        int skip = (int)(found - p);
        memcpy(out, p, skip);
        out += skip;
        memcpy(out, replacement, repLen);
        out += repLen;
        p = found + needleLen;
    }
    memcpy(out, p, strlen(p));
    buffer[outLen] = '\0';

    ObjString *result = copyString(buffer, outLen);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strRepeat(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    int times = (int)AS_NUMBER(args[1]);
    if (times <= 0)
        return OBJ_VAL(copyString("", 0));

    int outLen = str->length * times;
    char *buffer = (char *)malloc(outLen + 1);
    for (int i = 0; i < times; i++)
        memcpy(buffer + i * str->length, str->chars, str->length);
    buffer[outLen] = '\0';

    ObjString *result = copyString(buffer, outLen);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strReverse(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    ObjString *str = AS_STRING(args[0]);
    char *buffer = (char *)malloc(str->length + 1);
    for (int i = 0; i < str->length; i++)
        buffer[i] = str->chars[str->length - 1 - i];
    buffer[str->length] = '\0';

    ObjString *result = copyString(buffer, str->length);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strSplit(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    const char *src = AS_CSTRING(args[0]);
    const char *delim = AS_CSTRING(args[1]);
    int delimLen = AS_STRING(args[1])->length;

    ObjArray *result = newArray();
    push(OBJ_VAL(result));

    if (delimLen == 0)
    {
        pop();
        return NIL_VAL;
    }

    const char *p = src;
    const char *found;

    while ((found = strstr(p, delim)) != NULL)
    {
        int len = (int)(found - p);
        ObjString *part = copyString(p, len);
        push(OBJ_VAL(part));

        if (result->count == result->capacity)
        {
            int oldCap = result->capacity;
            result->capacity = GROW_CAPACITY(oldCap);
            result->values = GROW_ARRAY(Value, result->values, oldCap, result->capacity);
        }
        result->values[result->count++] = OBJ_VAL(part);
        pop();
        p = found + delimLen;
    }

    // last segment
    ObjString *last = copyString(p, (int)strlen(p));
    push(OBJ_VAL(last));
    if (result->count == result->capacity)
    {
        int oldCap = result->capacity;
        result->capacity = GROW_CAPACITY(oldCap);
        result->values = GROW_ARRAY(Value, result->values, oldCap, result->capacity);
    }
    result->values[result->count++] = OBJ_VAL(last);
    pop();

    pop(); // result GC guard
    return OBJ_VAL(result);
}

static Value strJoin(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    ObjArray *arr = AS_ARRAY(args[0]);
    ObjString *delim = AS_STRING(args[1]);

    if (arr->count == 0)
        return OBJ_VAL(copyString("", 0));

    // calculate total length
    int totalLen = 0;
    for (int i = 0; i < arr->count; i++)
    {
        if (!IS_STRING(arr->values[i]))
            return NIL_VAL;
        totalLen += AS_STRING(arr->values[i])->length;
    }
    totalLen += delim->length * (arr->count - 1);

    char *buffer = (char *)malloc(totalLen + 1);
    char *out = buffer;

    for (int i = 0; i < arr->count; i++)
    {
        ObjString *str = AS_STRING(arr->values[i]);
        memcpy(out, str->chars, str->length);
        out += str->length;
        if (i < arr->count - 1)
        {
            memcpy(out, delim->chars, delim->length);
            out += delim->length;
        }
    }
    *out = '\0';

    ObjString *result = copyString(buffer, totalLen);
    free(buffer);
    return OBJ_VAL(result);
}

static Value strFormat(int argCount, Value *args)
{
    if (argCount < 1 || !IS_STRING(args[0]))
        return NIL_VAL;

    ObjString *fmt = AS_STRING(args[0]);
    const char *src = fmt->chars;
    int srcLen = fmt->length;

    char buffer[65536];
    int outLen = 0;

    int i = 0;
    while (i < srcLen)
    {
        if (src[i] == '{' && i + 1 < srcLen)
        {
            // find closing }
            int j = i + 1;
            while (j < srcLen && src[j] != '}')
                j++;

            if (src[j] == '}')
            {
                // extract index number
                char numBuf[16] = {0};
                int numLen = j - i - 1;
                if (numLen > 0 && numLen < 16)
                {
                    memcpy(numBuf, src + i + 1, numLen);
                    int index = atoi(numBuf) + 1; // +1 because args[0] is format string

                    if (index < argCount)
                    {
                        // convert arg to string
                        Value arg = args[index];
                        char tmp[256];
                        int tmpLen = 0;

                        if (IS_STRING(arg))
                        {
                            ObjString *s = AS_STRING(arg);
                            memcpy(buffer + outLen, s->chars, s->length);
                            outLen += s->length;
                        }
                        else if (IS_NUMBER(arg))
                        {
                            tmpLen = snprintf(tmp, sizeof(tmp), "%g", AS_NUMBER(arg));
                            memcpy(buffer + outLen, tmp, tmpLen);
                            outLen += tmpLen;
                        }
                        else if (IS_BOOL(arg))
                        {
                            const char *b = AS_BOOL(arg) ? "true" : "false";
                            tmpLen = (int)strlen(b);
                            memcpy(buffer + outLen, b, tmpLen);
                            outLen += tmpLen;
                        }
                        else if (IS_NIL(arg))
                        {
                            memcpy(buffer + outLen, "nil", 3);
                            outLen += 3;
                        }
                    }
                    i = j + 1;
                    continue;
                }
            }
        }
        buffer[outLen++] = src[i++];
    }

    return OBJ_VAL(copyString(buffer, outLen));
}

ObjModule *initStrModule(void)
{
    ObjString *name = copyString("str", 3);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "upper", strUpper);
    setNative(module, "lower", strLower);
    setNative(module, "trim", strTrim);
    setNative(module, "trimLeft", strTrimLeft);
    setNative(module, "trimRight", strTrimRight);
    setNative(module, "len", strLen);
    setNative(module, "charAt", strCharAt);
    setNative(module, "indexOf", strIndexOf);
    setNative(module, "contains", strContains);
    setNative(module, "startsWith", strStartsWith);
    setNative(module, "endsWith", strEndsWith);
    setNative(module, "slice", strSlice);
    setNative(module, "replace", strReplace);
    setNative(module, "repeat", strRepeat);
    setNative(module, "reverse", strReverse);
    setNative(module, "split", strSplit);
    setNative(module, "join", strJoin);
    setNative(module, "format", strFormat);

    pop(); // module
    pop(); // name
    return module;
}
