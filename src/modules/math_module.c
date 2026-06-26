#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "math_module.h"
#include "../memory.h"
#include "../object.h"
#include "../value.h"
#include "../vm.h"
#include "../table.h"
#include <Accelerate/Accelerate.h>

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

static inline double mget(ObjMatrix *m, int i, int j)
{
    return m->data[i * m->cols + j];
}

static inline void mset(ObjMatrix *m, int i, int j, double val)
{
    m->data[i * m->cols + j] = val;
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

static Value mathMatmul(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_MATRIX(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *B = AS_MATRIX(args[1]);

    if (A->cols != B->rows)
        return NIL_VAL;

    int m = A->rows;
    int n = A->cols;
    int p = B->cols;

    ObjMatrix *C = newMatrix(m, p);
    push(OBJ_VAL(C));

    // C = A · B using BLAS
    cblas_dgemm(CblasRowMajor,
                CblasNoTrans,
                CblasNoTrans,
                m, p, n,
                1.0,
                A->data, n,
                B->data, p,
                0.0,
                C->data, p);

    pop();
    return OBJ_VAL(C);
}
static Value mathTranspose(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *T = newMatrix(A->cols, A->rows);
    push(OBJ_VAL(T));

    for (int i = 0; i < A->rows; i++)
        for (int j = 0; j < A->cols; j++)
            mset(T, j, i, mget(A, i, j));

    pop();
    return OBJ_VAL(T);
}

static Value mathZeros(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);

    ObjMatrix *m = newMatrix(rows, cols); // already zeroed
    push(OBJ_VAL(m));
    pop();
    return OBJ_VAL(m);
}

static Value mathOnes(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);

    ObjMatrix *m = newMatrix(rows, cols);
    push(OBJ_VAL(m));
    for (int i = 0; i < rows * cols; i++)
        m->data[i] = 1.0;
    pop();
    return OBJ_VAL(m);
}

static Value mathShape(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *m = AS_MATRIX(args[0]);

    ObjArray *shape = newArray();
    push(OBJ_VAL(shape));
    shape->capacity = 2;
    shape->values = ALLOCATE(Value, 2);
    shape->count = 2;
    shape->values[0] = NUMBER_VAL(m->rows);
    shape->values[1] = NUMBER_VAL(m->cols);
    pop();
    return OBJ_VAL(shape);
}

static Value mathSigmoid(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;

    if (IS_NUMBER(args[0]))
    {
        double x = AS_NUMBER(args[0]);
        return NUMBER_VAL(1.0 / (1.0 + exp(-x)));
    }

    if (IS_MATRIX(args[0]))
    {
        ObjMatrix *A = AS_MATRIX(args[0]);
        ObjMatrix *C = newMatrix(A->rows, A->cols);
        push(OBJ_VAL(C));
        for (int i = 0; i < A->rows * A->cols; i++)
            C->data[i] = 1.0 / (1.0 + exp(-A->data[i]));
        pop();
        return OBJ_VAL(C);
    }

    return NIL_VAL;
}

static Value mathRelu(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;

    if (IS_NUMBER(args[0]))
    {
        double x = AS_NUMBER(args[0]);
        return NUMBER_VAL(x > 0 ? x : 0.0);
    }

    if (IS_MATRIX(args[0]))
    {
        ObjMatrix *A = AS_MATRIX(args[0]);
        ObjMatrix *C = newMatrix(A->rows, A->cols);
        push(OBJ_VAL(C));
        for (int i = 0; i < A->rows * A->cols; i++)
            C->data[i] = A->data[i] > 0 ? A->data[i] : 0.0;
        pop();
        return OBJ_VAL(C);
    }

    return NIL_VAL;
}

static Value mathSoftmax(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;

    ObjArray *arr = AS_ARRAY(args[0]);
    int n = arr->count;

    // find max for numerical stability
    double maxVal = AS_NUMBER(arr->values[0]);
    for (int i = 1; i < n; i++)
    {
        double x = AS_NUMBER(arr->values[i]);
        if (x > maxVal)
            maxVal = x;
    }

    // compute exp(x - max) and sum
    double sum = 0.0;
    double *exps = (double *)malloc(n * sizeof(double));
    for (int i = 0; i < n; i++)
    {
        exps[i] = exp(AS_NUMBER(arr->values[i]) - maxVal);
        sum += exps[i];
    }

    // normalize
    ObjArray *result = newArray();
    push(OBJ_VAL(result));

    result->capacity = n;
    result->values = ALLOCATE(Value, n);
    result->count = n;

    for (int i = 0; i < n; i++)
        result->values[i] = NUMBER_VAL(exps[i] / sum);

    free(exps);
    pop();
    return OBJ_VAL(result);
}

static Value mathRandomMatrix(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);

    ObjMatrix *m = newMatrix(rows, cols);
    push(OBJ_VAL(m));

    double scale = sqrt(2.0 / (rows + cols));
    for (int i = 0; i < rows * cols; i++)
        m->data[i] = (((double)rand() / RAND_MAX) * 2.0 - 1.0) * scale;

    pop();
    return OBJ_VAL(m);
}

static Value mathMAdd(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_MATRIX(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *B = AS_MATRIX(args[1]);

    int rows = A->rows;
    int cols = A->cols;

    // B must match or be broadcastable
    if (B->rows != rows && B->rows != 1)
        return NIL_VAL;
    if (B->cols != cols && B->cols != 1)
        return NIL_VAL;

    ObjMatrix *C = newMatrix(rows, cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            double a = mget(A, i, j);
            double b = mget(B, B->rows == 1 ? 0 : i, B->cols == 1 ? 0 : j);
            mset(C, i, j, a + b);
        }

    pop();
    return OBJ_VAL(C);
}

static Value mathMSub(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_MATRIX(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *B = AS_MATRIX(args[1]);

    int rows = A->rows;
    int cols = A->cols;

    if (B->rows != rows && B->rows != 1)
        return NIL_VAL;
    if (B->cols != cols && B->cols != 1)
        return NIL_VAL;

    ObjMatrix *C = newMatrix(rows, cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            double a = mget(A, i, j);
            double b = mget(B, B->rows == 1 ? 0 : i, B->cols == 1 ? 0 : j);
            mset(C, i, j, a - b);
        }

    pop();
    return OBJ_VAL(C);
}

static Value mathMScale(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    double scalar = AS_NUMBER(args[1]);

    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < A->rows * A->cols; i++)
        C->data[i] = A->data[i] * scalar;

    pop();
    return OBJ_VAL(C);
}

static Value mathMMul(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_MATRIX(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *B = AS_MATRIX(args[1]);

    if (A->rows != B->rows || A->cols != B->cols)
        return NIL_VAL;

    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < A->rows * A->cols; i++)
        C->data[i] = A->data[i] * B->data[i];

    pop();
    return OBJ_VAL(C);
}

static Value mathMLog(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < A->rows * A->cols; i++)
        C->data[i] = log(A->data[i] + 1e-8);

    pop();
    return OBJ_VAL(C);
}

static Value mathMSum(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    int axis = (int)AS_NUMBER(args[1]);

    if (axis == 1)
    {
        // sum each row → (rows, 1)
        ObjMatrix *C = newMatrix(A->rows, 1);
        push(OBJ_VAL(C));
        for (int i = 0; i < A->rows; i++)
        {
            double sum = 0.0;
            for (int j = 0; j < A->cols; j++)
                sum += mget(A, i, j);
            mset(C, i, 0, sum);
        }
        pop();
        return OBJ_VAL(C);
    }
    else
    {
        // sum each col → (1, cols)
        ObjMatrix *C = newMatrix(1, A->cols);
        push(OBJ_VAL(C));
        for (int j = 0; j < A->cols; j++)
        {
            double sum = 0.0;
            for (int i = 0; i < A->rows; i++)
                sum += mget(A, i, j);
            mset(C, 0, j, sum);
        }
        pop();
        return OBJ_VAL(C);
    }
}

static Value mathMSigmoidDeriv(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < A->rows * A->cols; i++)
    {
        double s = A->data[i];
        C->data[i] = s * (1.0 - s);
    }

    pop();
    return OBJ_VAL(C);
}

static Value mathMReluDeriv(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    for (int i = 0; i < A->rows * A->cols; i++)
        C->data[i] = A->data[i] > 0 ? 1.0 : 0.0;

    pop();
    return OBJ_VAL(C);
}

static Value mathOneHot(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    ObjArray *labels = AS_ARRAY(args[0]);
    int numClasses = (int)AS_NUMBER(args[1]);
    int m = labels->count;

    ObjMatrix *result = newMatrix(m, numClasses);
    push(OBJ_VAL(result));

    for (int i = 0; i < m; i++)
    {
        int label = (int)AS_NUMBER(labels->values[i]);
        mset(result, i, label, 1.0);
    }

    pop();
    return OBJ_VAL(result);
}

static Value mathArgmax(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);

    // argmax per column → returns ObjArray of indices
    ObjArray *result = newArray();
    push(OBJ_VAL(result));
    result->capacity = A->cols;
    result->values = ALLOCATE(Value, A->cols);
    result->count = A->cols;

    for (int j = 0; j < A->cols; j++)
    {
        int maxIdx = 0;
        double maxVal = mget(A, 0, j);
        for (int i = 1; i < A->rows; i++)
        {
            double val = mget(A, i, j);
            if (val > maxVal)
            {
                maxVal = val;
                maxIdx = i;
            }
        }
        result->values[j] = NUMBER_VAL(maxIdx);
    }

    pop();
    return OBJ_VAL(result);
}

static Value mathFromArray(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;

    ObjArray *arr = AS_ARRAY(args[0]);
    if (arr->count == 0)
        return NIL_VAL;

    // 2D array → ObjMatrix
    if (IS_ARRAY(arr->values[0]))
    {
        int rows = arr->count;
        int cols = AS_ARRAY(arr->values[0])->count;
        ObjMatrix *m = newMatrix(rows, cols);
        push(OBJ_VAL(m));

        for (int i = 0; i < rows; i++)
        {
            ObjArray *row = AS_ARRAY(arr->values[i]);
            for (int j = 0; j < cols; j++)
                mset(m, i, j, AS_NUMBER(row->values[j]));
        }

        pop();
        return OBJ_VAL(m);
    }

    // 1D array → (n, 1) matrix
    int n = arr->count;
    ObjMatrix *m = newMatrix(n, 1);
    push(OBJ_VAL(m));
    for (int i = 0; i < n; i++)
        mset(m, i, 0, AS_NUMBER(arr->values[i]));
    pop();
    return OBJ_VAL(m);
}

static Value mathSaveMatrix(int argCount, Value *args)
{
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;

    ObjMatrix *m = AS_MATRIX(args[0]);
    const char *path = AS_CSTRING(args[1]);

    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        printf("Error: could not open '%s'\n", path);
        return BOOL_VAL(false);
    }

    // write header
    fwrite(&m->rows, sizeof(int), 1, f);
    fwrite(&m->cols, sizeof(int), 1, f);

    // write data
    fwrite(m->data, sizeof(double), m->rows * m->cols, f);

    fclose(f);
    return BOOL_VAL(true);
}

static Value mathLoadMatrix(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;

    const char *path = AS_CSTRING(args[0]);

    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        printf("Error: could not open '%s'\n", path);
        return NIL_VAL;
    }

    int rows, cols;
    fread(&rows, sizeof(int), 1, f);
    fread(&cols, sizeof(int), 1, f);

    ObjMatrix *m = newMatrix(rows, cols);
    push(OBJ_VAL(m));

    fread(m->data, sizeof(double), rows * cols, f);
    fclose(f);

    pop();
    return OBJ_VAL(m);
}

static Value mathMSoftmax(int argCount, Value *args)
{
    if (argCount != 1 || !IS_MATRIX(args[0]))
        return NIL_VAL;

    ObjMatrix *A = AS_MATRIX(args[0]);
    ObjMatrix *C = newMatrix(A->rows, A->cols);
    push(OBJ_VAL(C));

    // softmax per column
    for (int j = 0; j < A->cols; j++)
    {
        // find max for numerical stability
        double maxVal = mget(A, 0, j);
        for (int i = 1; i < A->rows; i++)
        {
            double val = mget(A, i, j);
            if (val > maxVal)
                maxVal = val;
        }

        // compute exp and sum
        double sum = 0.0;
        for (int i = 0; i < A->rows; i++)
        {
            double e = exp(mget(A, i, j) - maxVal);
            mset(C, i, j, e);
            sum += e;
        }

        // normalize
        for (int i = 0; i < A->rows; i++)
            mset(C, i, j, mget(C, i, j) / sum);
    }

    pop();
    return OBJ_VAL(C);
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
    setNative(module, "matmul", mathMatmul);
    setNative(module, "transpose", mathTranspose);
    setNative(module, "zeros", mathZeros);
    setNative(module, "ones", mathOnes);
    setNative(module, "shape", mathShape);
    setNative(module, "sigmoid", mathSigmoid);
    setNative(module, "relu", mathRelu);
    setNative(module, "softmax", mathSoftmax);
    setNative(module, "randomMatrix", mathRandomMatrix);
    setNative(module, "madd", mathMAdd);
    setNative(module, "msub", mathMSub);
    setNative(module, "mscale", mathMScale);
    setNative(module, "mmul", mathMMul);
    setNative(module, "mlog", mathMLog);
    setNative(module, "msum", mathMSum);
    setNative(module, "msigmoidDeriv", mathMSigmoidDeriv);
    setNative(module, "msoftmax", mathMSoftmax);
    setNative(module, "mreluDeriv", mathMReluDeriv);
    setNative(module, "oneHot", mathOneHot);
    setNative(module, "argmax", mathArgmax);
    setNative(module, "fromArray", mathFromArray);
    setNative(module, "saveMatrix", mathSaveMatrix);
    setNative(module, "loadMatrix", mathLoadMatrix);

    setNumber(module, "pi", 3.14159265358979323846);
    setNumber(module, "e", 2.71828182845904523536);
    setNumber(module, "inf", 1.0 / 0.0);
    setNumber(module, "nan", 0.0 / 0.0);

    pop(); // module
    pop(); // name
    return module;
}
