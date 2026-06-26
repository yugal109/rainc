#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "time_module.h"
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

static Value timeNow(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return NUMBER_VAL((double)time(NULL));
}

static Value timeDate(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return OBJ_VAL(copyString(buf, (int)strlen(buf)));
}

static Value timeClock(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return OBJ_VAL(copyString(buf, (int)strlen(buf)));
}

static Value timeDatetime(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return OBJ_VAL(copyString(buf, (int)strlen(buf)));
}

static Value timeYear(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_year + 1900);
}

static Value timeMonth(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_mon + 1);
}

static Value timeDay(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_mday);
}

static Value timeHour(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_hour);
}

static Value timeMinute(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_min);
}

static Value timeSecond(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return NUMBER_VAL(tm->tm_sec);
}

static Value timeFormat(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    time_t t = (time_t)AS_NUMBER(args[0]);
    const char *fmt = AS_CSTRING(args[1]);
    struct tm *tm = localtime(&t);
    char buf[256];
    strftime(buf, sizeof(buf), fmt, tm);
    return OBJ_VAL(copyString(buf, (int)strlen(buf)));
}

static Value timeWeekday(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char *day = days[tm->tm_wday];
    return OBJ_VAL(copyString(day, (int)strlen(day)));
}

static Value timeMonthName(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    const char *months[] = {"January", "February", "March", "April", "May", "June",
                            "July", "August", "September", "October", "November", "December"};
    const char *month = months[tm->tm_mon];
    return OBJ_VAL(copyString(month, (int)strlen(month)));
}

ObjModule *initTimeModule(void)
{
    ObjString *name = copyString("time", 4);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "now", timeNow);
    setNative(module, "date", timeDate);
    setNative(module, "clock", timeClock);
    setNative(module, "datetime", timeDatetime);
    setNative(module, "year", timeYear);
    setNative(module, "month", timeMonth);
    setNative(module, "day", timeDay);
    setNative(module, "hour", timeHour);
    setNative(module, "minute", timeMinute);
    setNative(module, "second", timeSecond);
    setNative(module, "format", timeFormat);
    setNative(module, "weekday", timeWeekday);
    setNative(module, "monthName", timeMonthName);

    pop(); // module
    pop(); // name
    return module;
}
