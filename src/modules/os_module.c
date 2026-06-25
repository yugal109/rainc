#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include "os_module.h"
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

static Value osCwd(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == NULL)
        return NIL_VAL;
    return OBJ_VAL(copyString(buffer, (int)strlen(buffer)));
}

static Value osPlatform(int argCount, Value *args)
{
    (void)argCount;
    (void)args;

#ifdef _WIN32
    return OBJ_VAL(copyString("windows", 7));
#elif __APPLE__
    return OBJ_VAL(copyString("macos", 5));
#else
    return OBJ_VAL(copyString("linux", 5));
#endif
}

static Value osExit(int argCount, Value *args)
{
    int code = 0;
    if (argCount == 1 && IS_NUMBER(args[0]))
        code = (int)AS_NUMBER(args[0]);
    exit(code);
    return NIL_VAL;
}

static Value osTime(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return NUMBER_VAL((double)time(NULL));
}

static Value osClock(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value osSleep(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    int ms = (int)AS_NUMBER(args[0]);
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
    return NIL_VAL;
}

static Value osEnv(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *name = AS_CSTRING(args[0]);
    const char *value = getenv(name);
    if (value == NULL)
        return NIL_VAL;
    return OBJ_VAL(copyString(value, (int)strlen(value)));
}

ObjModule *initOsModule(void)
{
    ObjString *name = copyString("os", 2);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "cwd", osCwd);
    setNative(module, "platform", osPlatform);
    setNative(module, "exit", osExit);
    setNative(module, "time", osTime);
    setNative(module, "clock", osClock);
    setNative(module, "sleep", osSleep);
    setNative(module, "env", osEnv);

    pop(); // module
    pop(); // name
    return module;
}
