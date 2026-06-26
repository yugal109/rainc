#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv_module.h"
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

static Value csvRead(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *path = AS_CSTRING(args[0]);

    // open file
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("Error: could not open '%s'\n", path);
        return NIL_VAL;
    }

    // result array of rows
    ObjArray *result = newArray();
    push(OBJ_VAL(result));

    char line[65536]; // buffer for one line
    while (fgets(line, sizeof(line), file) != NULL)
    {
        // strip newline
        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r')
            line[--len] = '\0';
        if (len == 0)
            continue;

        // parse row — split by comma
        ObjArray *row = newArray();
        push(OBJ_VAL(row));

        char *p = line;
        char *start = line;
        int colCount = 0;

        // count columns first
        for (int i = 0; i <= len; i++)
        {
            if (line[i] == ',' || line[i] == '\0')
                colCount++;
        }

        // allocate row
        row->capacity = colCount;
        row->values = ALLOCATE(Value, colCount);
        row->count = 0;

        // parse values
        p = line;
        while (1)
        {
            char *comma = strchr(p, ',');
            if (comma != NULL)
                *comma = '\0';

            // convert to number
            double val = strtod(p, NULL);
            row->values[row->count++] = NUMBER_VAL(val);

            if (comma == NULL)
                break;
            p = comma + 1;
        }

        // add row to result
        if (result->count == result->capacity)
        {
            int oldCap = result->capacity;
            result->capacity = GROW_CAPACITY(oldCap);
            result->values = GROW_ARRAY(Value, result->values, oldCap, result->capacity);
        }
        result->values[result->count++] = OBJ_VAL(row);
        pop(); // row
    }

    fclose(file);
    pop(); // result
    return OBJ_VAL(result);
}

static Value csvShape(int argCount, Value *args)
{
    if (argCount != 1 || !IS_ARRAY(args[0]))
        return NIL_VAL;
    ObjArray *data = AS_ARRAY(args[0]);
    int rows = data->count;
    int cols = 0;
    if (rows > 0 && IS_ARRAY(data->values[0]))
        cols = AS_ARRAY(data->values[0])->count;

    ObjArray *shape = newArray();
    push(OBJ_VAL(shape));
    shape->capacity = 2;
    shape->values = ALLOCATE(Value, 2);
    shape->count = 2;
    shape->values[0] = NUMBER_VAL(rows);
    shape->values[1] = NUMBER_VAL(cols);
    pop();
    return OBJ_VAL(shape);
}

static Value csvCol(int argCount, Value *args)
{
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1]))
        return NIL_VAL;

    ObjArray *data = AS_ARRAY(args[0]);
    int col = (int)AS_NUMBER(args[1]);
    int rows = data->count;

    ObjArray *result = newArray();
    push(OBJ_VAL(result));
    result->capacity = rows;
    result->values = ALLOCATE(Value, rows);
    result->count = rows;

    for (int i = 0; i < rows; i++)
    {
        ObjArray *row = AS_ARRAY(data->values[i]);
        result->values[i] = row->values[col];
    }

    pop();
    return OBJ_VAL(result);
}

static Value csvSliceCols(int argCount, Value *args)
{
    if (argCount != 3 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
        return NIL_VAL;

    ObjArray *data = AS_ARRAY(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int end = (int)AS_NUMBER(args[2]);
    int rows = data->count;
    int cols = end - start;

    ObjArray *result = newArray();
    push(OBJ_VAL(result));
    result->capacity = rows;
    result->values = ALLOCATE(Value, rows);
    result->count = rows;

    for (int i = 0; i < rows; i++)
    {
        ObjArray *srcRow = AS_ARRAY(data->values[i]);
        ObjArray *row = newArray();
        push(OBJ_VAL(row));
        row->capacity = cols;
        row->values = ALLOCATE(Value, cols);
        row->count = cols;
        for (int j = 0; j < cols; j++)
            row->values[j] = srcRow->values[start + j];
        result->values[i] = OBJ_VAL(row);
        pop();
    }

    pop();
    return OBJ_VAL(result);
}

static Value csvSliceRows(int argCount, Value *args)
{
    if (argCount != 3 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2]))
        return NIL_VAL;

    ObjArray *data = AS_ARRAY(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int end = (int)AS_NUMBER(args[2]);

    if (start < 0)
        start = 0;
    if (end > data->count)
        end = data->count;
    int count = end - start;

    ObjArray *result = newArray();
    push(OBJ_VAL(result));
    result->capacity = count;
    result->values = ALLOCATE(Value, count);
    result->count = count;

    for (int i = 0; i < count; i++)
        result->values[i] = data->values[start + i];

    pop();
    return OBJ_VAL(result);
}

ObjModule *initCsvModule(void)
{
    ObjString *name = copyString("csv", 3);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "read", csvRead);
    setNative(module, "shape", csvShape);
    setNative(module, "col", csvCol);
    setNative(module, "sliceCols", csvSliceCols);
    setNative(module, "sliceRows", csvSliceRows);

    pop(); // module
    pop(); // name
    return module;
}
