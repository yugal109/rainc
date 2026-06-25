#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "math_module.h"
#include "../memory.h"
#include "../object.h"
#include "../value.h"
#include "../vm.h"
#include "../table.h"

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

static void setNumber(ObjModule *module, const char *name, double value)
{
    ObjString *key = copyString(name, (int)strlen(name));
    push(OBJ_VAL(key));
    tableSet(&module->fields, key, NUMBER_VAL(value));
    pop();
}

static Value mathSin(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static Value mathCos(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static Value mathTan(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

static Value mathSqrt(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(sqrt(AS_NUMBER(args[0])));
}

static Value mathPow(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value mathAbs(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(fabs(AS_NUMBER(args[0])));
}

static Value mathFloor(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

static Value mathCeil(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

static Value mathRound(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static Value mathLog(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(log(AS_NUMBER(args[0])));
}

static Value mathLog2(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(log2(AS_NUMBER(args[0])));
}

static Value mathLog10(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(log10(AS_NUMBER(args[0])));
}

static Value mathMax(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(AS_NUMBER(args[0]) > AS_NUMBER(args[1]) ? AS_NUMBER(args[0]) : AS_NUMBER(args[1]));
}

static Value mathMin(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(AS_NUMBER(args[0]) < AS_NUMBER(args[1]) ? AS_NUMBER(args[0]) : AS_NUMBER(args[1]));
}

static Value mathEmul(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_ARRAY(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    ObjArray *b = AS_ARRAY(args[1]);
    if (a->count != b->count)
    {
        return NIL_VAL;
    }
    ObjArray *result = newArray();
    result->capacity = a->count;
    result->count = a->count;
    result->values = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]) || !IS_NUMBER(b->values[i]))
            return NIL_VAL;
        result->values[i] = NUMBER_VAL(AS_NUMBER(a->values[i]) * AS_NUMBER(b->values[i]));
    }
    return OBJ_VAL(result);
}

static Value mathEdiv(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_ARRAY(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    ObjArray *b = AS_ARRAY(args[1]);
    if (a->count != b->count)
        return NIL_VAL;
    ObjArray *result = newArray();
    result->capacity = a->count;
    result->count = a->count;
    result->values = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]) || !IS_NUMBER(b->values[i]))
            return NIL_VAL;
        if (AS_NUMBER(b->values[i]) == 0)
            return NIL_VAL;
        result->values[i] = NUMBER_VAL(AS_NUMBER(a->values[i]) / AS_NUMBER(b->values[i]));
    }
    return OBJ_VAL(result);
}

static Value mathEadd(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_ARRAY(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    ObjArray *b = AS_ARRAY(args[1]);
    if (a->count != b->count)
        return NIL_VAL;
    ObjArray *result = newArray();
    result->capacity = a->count;
    result->count = a->count;
    result->values = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]) || !IS_NUMBER(b->values[i]))
            return NIL_VAL;
        result->values[i] = NUMBER_VAL(AS_NUMBER(a->values[i]) + AS_NUMBER(b->values[i]));
    }
    return OBJ_VAL(result);
}

static Value mathEsub(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_ARRAY(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    ObjArray *b = AS_ARRAY(args[1]);
    if (a->count != b->count)
        return NIL_VAL;
    ObjArray *result = newArray();
    result->capacity = a->count;
    result->count = a->count;
    result->values = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]) || !IS_NUMBER(b->values[i]))
            return NIL_VAL;
        result->values[i] = NUMBER_VAL(AS_NUMBER(a->values[i]) - AS_NUMBER(b->values[i]));
    }
    return OBJ_VAL(result);
}

static Value mathDot(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_ARRAY(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    ObjArray *b = AS_ARRAY(args[1]);
    if (a->count != b->count)
        return NIL_VAL;
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]) || !IS_NUMBER(b->values[i]))
            return NIL_VAL;
        sum += AS_NUMBER(a->values[i]) * AS_NUMBER(b->values[i]);
    }
    return NUMBER_VAL(sum);
}

static Value mathSum(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        sum += AS_NUMBER(a->values[i]);
    }
    return NUMBER_VAL(sum);
}

static Value mathMean(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    if (a->count == 0)
        return NIL_VAL;
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        sum += AS_NUMBER(a->values[i]);
    }
    return NUMBER_VAL(sum / a->count);
}

static Value mathNorm(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        double v = AS_NUMBER(a->values[i]);
        sum += v * v;
    }
    return NUMBER_VAL(sqrt(sum));
}

static Value mathScale(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    double scalar = AS_NUMBER(args[1]);
    ObjArray *result = newArray();
    result->capacity = a->count;
    result->count = a->count;
    result->values = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        result->values[i] = NUMBER_VAL(AS_NUMBER(a->values[i]) * scalar);
    }
    return OBJ_VAL(result);
}

static Value mathAsin(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(asin(AS_NUMBER(args[0])));
}

static Value mathAcos(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(acos(AS_NUMBER(args[0])));
}

static Value mathAtan(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(atan(AS_NUMBER(args[0])));
}

static Value mathAtan2(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value mathSinh(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(sinh(AS_NUMBER(args[0])));
}

static Value mathCosh(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(cosh(AS_NUMBER(args[0])));
}

static Value mathTanh(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(tanh(AS_NUMBER(args[0])));
}

static Value mathTrunc(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(trunc(AS_NUMBER(args[0])));
}

static Value mathFmod(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(fmod(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value mathExp(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(exp(AS_NUMBER(args[0])));
}

static Value mathExp2(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(exp2(AS_NUMBER(args[0])));
}

static Value mathCbrt(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    return NUMBER_VAL(cbrt(AS_NUMBER(args[0])));
}

static Value mathHypot(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;
    return NUMBER_VAL(hypot(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value mathSign(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    double x = AS_NUMBER(args[0]);
    if (x > 0)
        return NUMBER_VAL(1);
    if (x < 0)
        return NUMBER_VAL(-1);
    return NUMBER_VAL(0);
}

static Value mathClamp(int argCount, Value *args)
{
    if (argCount != 3 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
        return NIL_VAL;
    double x = AS_NUMBER(args[0]);
    double min = AS_NUMBER(args[1]);
    double max = AS_NUMBER(args[2]);
    if (x < min)
        return NUMBER_VAL(min);
    if (x > max)
        return NUMBER_VAL(max);
    return NUMBER_VAL(x);
}

static Value mathRandom(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return NUMBER_VAL((double)rand() / (double)RAND_MAX);
}

static Value mathRandomInt(int argCount, Value *args)
{
    if (argCount != 1 || !IS_NUMBER(args[0]))
        return NIL_VAL;
    int n = (int)AS_NUMBER(args[0]);
    if (n <= 0)
        return NIL_VAL;
    return NUMBER_VAL(rand() % n);
}

static Value mathStd(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    if (a->count == 0)
        return NIL_VAL;
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        sum += AS_NUMBER(a->values[i]);
    }
    double mean = sum / a->count;
    double variance = 0;
    for (int i = 0; i < a->count; i++)
    {
        double diff = AS_NUMBER(a->values[i]) - mean;
        variance += diff * diff;
    }
    return NUMBER_VAL(sqrt(variance / a->count));
}

static Value mathVariance(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    if (a->count == 0)
        return NIL_VAL;
    double sum = 0;
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
            return NIL_VAL;
        sum += AS_NUMBER(a->values[i]);
    }
    double mean = sum / a->count;
    double variance = 0;
    for (int i = 0; i < a->count; i++)
    {
        double diff = AS_NUMBER(a->values[i]) - mean;
        variance += diff * diff;
    }
    return NUMBER_VAL(variance / a->count);
}

static Value mathMedian(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *a = AS_ARRAY(args[0]);
    if (a->count == 0)
        return NIL_VAL;

    // copy values to temp array and sort
    Value *temp = ALLOCATE(Value, a->count);
    for (int i = 0; i < a->count; i++)
    {
        if (!IS_NUMBER(a->values[i]))
        {
            FREE_ARRAY(Value, temp, a->count);
            return NIL_VAL;
        }
        temp[i] = a->values[i];
    }
    // simple insertion sort
    for (int i = 1; i < a->count; i++)
    {
        Value key = temp[i];
        int j = i - 1;
        while (j >= 0 && AS_NUMBER(temp[j]) > AS_NUMBER(key))
        {
            temp[j + 1] = temp[j];
            j--;
        }
        temp[j + 1] = key;
    }
    double median;
    if (a->count % 2 == 0)
        median = (AS_NUMBER(temp[a->count / 2 - 1]) + AS_NUMBER(temp[a->count / 2])) / 2.0;
    else
        median = AS_NUMBER(temp[a->count / 2]);

    FREE_ARRAY(Value, temp, a->count);
    return NUMBER_VAL(median);
}
ObjModule *initMathModule(void)
{
    ObjString *name = copyString("math", 4);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "sin", mathSin);
    setNative(module, "cos", mathCos);
    setNative(module, "tan", mathTan);
    setNative(module, "sqrt", mathSqrt);
    setNative(module, "pow", mathPow);
    setNative(module, "abs", mathAbs);
    setNative(module, "floor", mathFloor);
    setNative(module, "ceil", mathCeil);
    setNative(module, "round", mathRound);
    setNative(module, "log", mathLog);
    setNative(module, "log2", mathLog2);
    setNative(module, "log10", mathLog10);
    setNative(module, "max", mathMax);
    setNative(module, "min", mathMin);
    setNative(module, "emul", mathEmul);
    setNative(module, "ediv", mathEdiv);
    setNative(module, "eadd", mathEadd);
    setNative(module, "esub", mathEsub);
    setNative(module, "dot", mathDot);
    setNative(module, "sum", mathSum);
    setNative(module, "mean", mathMean);
    setNative(module, "norm", mathNorm);
    setNative(module, "scale", mathScale);
    setNative(module, "asin", mathAsin);
    setNative(module, "acos", mathAcos);
    setNative(module, "atan", mathAtan);
    setNative(module, "atan2", mathAtan2);
    setNative(module, "sinh", mathSinh);
    setNative(module, "cosh", mathCosh);
    setNative(module, "tanh", mathTanh);
    setNative(module, "trunc", mathTrunc);
    setNative(module, "fmod", mathFmod);
    setNative(module, "exp", mathExp);
    setNative(module, "exp2", mathExp2);
    setNative(module, "cbrt", mathCbrt);
    setNative(module, "hypot", mathHypot);
    setNative(module, "sign", mathSign);
    setNative(module, "clamp", mathClamp);
    setNative(module, "random", mathRandom);
    setNative(module, "randomInt", mathRandomInt);
    setNative(module, "std", mathStd);
    setNative(module, "variance", mathVariance);
    setNative(module, "median", mathMedian);

    setNumber(module, "pi", 3.14159265358979323846);
    setNumber(module, "e", 2.71828182845904523536);
    setNumber(module, "inf", 1.0 / 0.0);
    setNumber(module, "nan", 0.0 / 0.0);

    pop(); // module
    pop(); // name
    return module;
}
