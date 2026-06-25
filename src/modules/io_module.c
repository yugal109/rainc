#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_module.h"
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

static Value ioReadFile(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *path = AS_CSTRING(args[0]);

    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NIL_VAL;

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL)
    {
        fclose(file);
        return NIL_VAL;
    }

    size_t bytesRead = fread(buffer, sizeof(char), size, file);
    buffer[bytesRead] = '\0';
    fclose(file);

    ObjString *result = copyString(buffer, (int)bytesRead);
    free(buffer);
    return OBJ_VAL(result);
}

static Value ioWriteFile(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    const char *path = AS_CSTRING(args[0]);
    const char *content = AS_CSTRING(args[1]);

    FILE *file = fopen(path, "w");
    if (file == NULL)
        return BOOL_VAL(false);

    fputs(content, file);
    fclose(file);
    return BOOL_VAL(true);
}

static Value ioInput(int argCount, Value *args)
{
    if (argCount == 1 && IS_STRING(args[0]))
        printf("%s", AS_CSTRING(args[0]));

    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return NIL_VAL;

    int len = (int)strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = '\0';

    return OBJ_VAL(copyString(buffer, (int)strlen(buffer)));
}

static Value ioInputNumber(int argCount, Value *args)
{
    if (argCount == 1 && IS_STRING(args[0]))
        printf("%s", AS_CSTRING(args[0]));

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return NIL_VAL;

    double value = strtod(buffer, NULL);
    return NUMBER_VAL(value);
}
static Value ioFileExists(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;
    const char *path = AS_CSTRING(args[0]);
    FILE *file = fopen(path, "r");
    if (file == NULL) return BOOL_VAL(false);
    fclose(file);
    return BOOL_VAL(true);
}

static Value ioDeleteFile(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;
    const char *path = AS_CSTRING(args[0]);
    return BOOL_VAL(remove(path) == 0);
}

ObjModule *initIoModule(void)
{
    ObjString *name = copyString("io", 2);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "readFile", ioReadFile);
    setNative(module, "writeFile", ioWriteFile);
    setNative(module, "fileExists", ioFileExists);
    setNative(module, "deleteFile", ioDeleteFile);
    setNative(module, "input", ioInput);
    setNative(module, "inputNumber", ioInputNumber);

    pop(); // module
    pop(); // name
    return module;
}
