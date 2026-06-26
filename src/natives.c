#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "natives.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "math_module.h"
#include "io_module.h"
#include "str_module.h"
#include "os_module.h"
#include "json_module.h"
#include "http_module.h"
#include "time_module.h"
#include "db_module.h"
#include "csv_module.h"
#include "compiler.h"

void defineNative(const char *name, NativeFn function)
{
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

static Value clockNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void registerModuleLoader(const char *name, NativeFn loader)
{
    ObjString *key = copyString(name, (int)strlen(name));
    push(OBJ_VAL(key));
    ObjNative *native = newNative(loader);
    push(OBJ_VAL(native));
    tableSet(&vm.preload, key, OBJ_VAL(native));
    pop();
    pop();
}

static char *readFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

static ObjString *moduleNameFromPath(const char *path, int length)
{
    int start = 0;
    for (int i = length - 1; i >= 0; i--)
    {
        // find last '/' or start

        if (path[i] == '/')
        {
            start = i + 1;
            break;
        }
    }
    int end = length;
    if (length > 3 && path[length - 3] == '.' && path[length - 2] == 'r' && path[length - 1] == 'n')
    {
        end = length - 3;
    }
    return copyString(path + start, end - start);
}

static Value importNative(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;

    ObjString *path = AS_STRING(args[0]);
    int pathLen = path->length;

    bool isFile = pathLen > 3 &&
                  path->chars[pathLen - 3] == '.' &&
                  path->chars[pathLen - 2] == 'r' &&
                  path->chars[pathLen - 1] == 'n';

    if (isFile)
    {
        ObjString *moduleName = moduleNameFromPath(path->chars, pathLen);

        Value cached;
        if (tableGet(&vm.importedModules, moduleName, &cached))
            return cached;

        Value loading;
        if (tableGet(&vm.loadingModules, moduleName, &loading))
            return NIL_VAL;

        tableSet(&vm.loadingModules, moduleName, BOOL_VAL(true));

        char *source = readFile(path->chars);
        if (source == NULL)
        {
            tableDelete(&vm.loadingModules, moduleName);
            return NIL_VAL;
        }

        ObjModule *module = newModule(moduleName);
        push(OBJ_VAL(module));

        ObjFunction *function = compile(source);
        free(source);

        if (function == NULL)
        {
            pop();
            tableDelete(&vm.loadingModules, moduleName);
            return NIL_VAL;
        }

        ObjClosure *closure = newClosure(function);
        push(OBJ_VAL(closure));

        Table snapshot;
        initTable(&snapshot);
        for (int i = 0; i <= vm.globals.capacity; i++)
        {
            Entry *entry = &vm.globals.entries[i];
            if (entry->key == NULL)
                continue;
            tableSet(&snapshot, entry->key, entry->value);
        }

        int frameBase = vm.frameCount;
        callFunction(closure, 0);

        InterpretResult result = run(frameBase);

        if (result != INTERPRET_OK)
        {
            pop(); // closure
            pop(); // module
            tableDelete(&vm.loadingModules, moduleName);
            return NIL_VAL;
        }

        for (int i = 0; i <= vm.globals.capacity; i++)
        {
            Entry *entry = &vm.globals.entries[i];
            if (entry->key == NULL)
                continue;
            Value existing;
            if (tableGet(&snapshot, entry->key, &existing))
                continue;
            tableSet(&module->fields, entry->key, entry->value);
        }

        freeTable(&snapshot);

        tableSet(&vm.importedModules, moduleName, OBJ_VAL(module));
        tableDelete(&vm.loadingModules, moduleName);

        pop(); // module init return value
        pop(); // module
        return OBJ_VAL(module);
    }
    else
    {
        Value cached;
        if (tableGet(&vm.importedModules, path, &cached))
            return cached;

        Value loader;
        if (!tableGet(&vm.preload, path, &loader))
            return NIL_VAL;

        NativeFn loaderFn = AS_NATIVE(loader);
        Value module = loaderFn(0, NULL);

        push(module);
        tableSet(&vm.importedModules, path, module);
        pop();
        return module;
    }
}

static Value mathLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initMathModule());
}

static Value ioLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initIoModule());
}

static Value strLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initStrModule());
}

static Value osLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initOsModule());
}

static Value jsonLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initJsonModule());
}

static Value httpLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initHttpModule());
}

static Value timeLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initTimeModule());
}

static Value dbLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initDbModule());
}

static Value csvLoaderNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return OBJ_VAL(initCsvModule());
}

void registerNatives(void)
{
    defineNative("clock", clockNative);
    defineNative("__import__", importNative);

    registerModuleLoader("math", mathLoaderNative);
    registerModuleLoader("io", ioLoaderNative);
    registerModuleLoader("str", strLoaderNative);
    registerModuleLoader("os", osLoaderNative);
    registerModuleLoader("json", jsonLoaderNative);
    registerModuleLoader("http", httpLoaderNative);
    registerModuleLoader("time", timeLoaderNative);
    registerModuleLoader("db", dbLoaderNative);
    registerModuleLoader("csv", csvLoaderNative);
}
