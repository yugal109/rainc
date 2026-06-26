#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "db_module.h"
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

static void setField(ObjInstance *instance, const char *name, Value value)
{
    ObjString *key = copyString(name, (int)strlen(name));
    push(OBJ_VAL(key));
    push(value);
    tableSet(&instance->fields, key, value);
    pop();
    pop();
}

static sqlite3 *getConnection(Value val)
{
    if (!IS_INSTANCE(val))
        return NULL;
    ObjInstance *conn = AS_INSTANCE(val);
    Value ptr;
    ObjString *key = copyString("__ptr", 5);
    if (!tableGet(&conn->fields, key, &ptr))
        return NULL;
    if (!IS_NUMBER(ptr))
        return NULL;
    return (sqlite3 *)(uintptr_t)AS_NUMBER(ptr);
}

static ObjInstance *makeObject(void)
{
    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *instance = newInstance(klass);
    push(OBJ_VAL(instance));
    pop();
    pop();
    return instance;
}

static Value dbOpen(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;

    const char *path = AS_CSTRING(args[0]);
    const char *engine = AS_CSTRING(args[1]);

    // only sqlite supported for now
    if (strcmp(engine, "sqlite") != 0)
    {
        printf("Error: unsupported engine '%s'. Only 'sqlite' is supported.\n", engine);
        return NIL_VAL;
    }

    // open sqlite database
    sqlite3 *db;
    if (sqlite3_open(path, &db) != SQLITE_OK)
    {
        printf("Error: could not open '%s': %s\n", path, sqlite3_errmsg(db));
        sqlite3_close(db);
        return NIL_VAL;
    }

    // wrap in ObjInstance
    ObjInstance *conn = makeObject();
    push(OBJ_VAL(conn));

    setField(conn, "__ptr", NUMBER_VAL((double)(uintptr_t)db));
    setField(conn, "__type", OBJ_VAL(copyString("sqlite3", 7)));
    setField(conn, "path", OBJ_VAL(copyString(path, (int)strlen(path))));
    setField(conn, "engine", OBJ_VAL(copyString(engine, (int)strlen(engine))));

    pop(); // conn
    return OBJ_VAL(conn);
}

static Value dbExec(int argCount, Value *args)
{
    if (argCount < 2 || !IS_STRING(args[1]))
        return NIL_VAL;

    sqlite3 *db = getConnection(args[0]);
    if (db == NULL)
    {
        printf("Error: invalid connection\n");
        return BOOL_VAL(false);
    }

    const char *sql = AS_CSTRING(args[1]);

    // no params — simple exec
    if (argCount == 2)
    {
        char *errMsg = NULL;
        int rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
        if (rc != SQLITE_OK)
        {
            printf("Error: %s\n", errMsg);
            sqlite3_free(errMsg);
            return BOOL_VAL(false);
        }
        return BOOL_VAL(true);
    }

    // with params — prepared statement
    if (argCount == 3 && IS_ARRAY(args[2]))
    {
        ObjArray *params = AS_ARRAY(args[2]);

        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            printf("Error: %s\n", sqlite3_errmsg(db));
            return BOOL_VAL(false);
        }

        // bind params
        for (int i = 0; i < params->count; i++)
        {
            Value param = params->values[i];
            if (IS_NUMBER(param))
            {
                sqlite3_bind_double(stmt, i + 1, AS_NUMBER(param));
            }
            else if (IS_STRING(param))
            {
                sqlite3_bind_text(stmt, i + 1, AS_CSTRING(param), -1, SQLITE_STATIC);
            }
            else if (IS_BOOL(param))
            {
                sqlite3_bind_int(stmt, i + 1, AS_BOOL(param) ? 1 : 0);
            }
            else if (IS_NIL(param))
            {
                sqlite3_bind_null(stmt, i + 1);
            }
        }

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        {
            printf("Error: %s\n", sqlite3_errmsg(db));
            return BOOL_VAL(false);
        }
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}

static Value dbQuery(int argCount, Value *args)
{
    if (argCount < 2 || !IS_STRING(args[1]))
        return NIL_VAL;

    sqlite3 *db = getConnection(args[0]);
    if (db == NULL)
    {
        printf("Error: invalid connection\n");
        return NIL_VAL;
    }

    const char *sql = AS_CSTRING(args[1]);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error: %s\n", sqlite3_errmsg(db));
        return NIL_VAL;
    }

    // bind params if provided
    if (argCount == 3 && IS_ARRAY(args[2]))
    {
        ObjArray *params = AS_ARRAY(args[2]);
        for (int i = 0; i < params->count; i++)
        {
            Value param = params->values[i];
            if (IS_NUMBER(param))
                sqlite3_bind_double(stmt, i + 1, AS_NUMBER(param));
            else if (IS_STRING(param))
                sqlite3_bind_text(stmt, i + 1, AS_CSTRING(param), -1, SQLITE_STATIC);
            else if (IS_BOOL(param))
                sqlite3_bind_int(stmt, i + 1, AS_BOOL(param) ? 1 : 0);
            else if (IS_NIL(param))
                sqlite3_bind_null(stmt, i + 1);
        }
    }

    // build result array
    ObjArray *results = newArray();
    push(OBJ_VAL(results));

    int colCount = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // each row becomes an ObjInstance
        ObjInstance *row = makeObject();
        push(OBJ_VAL(row));

        for (int i = 0; i < colCount; i++)
        {
            const char *colName = sqlite3_column_name(stmt, i);
            int colType = sqlite3_column_type(stmt, i);

            Value val;
            switch (colType)
            {
            case SQLITE_INTEGER:
                val = NUMBER_VAL((double)sqlite3_column_int64(stmt, i));
                break;
            case SQLITE_FLOAT:
                val = NUMBER_VAL(sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT:
            {
                const char *text = (const char *)sqlite3_column_text(stmt, i);
                val = OBJ_VAL(copyString(text, (int)strlen(text)));
                break;
            }
            case SQLITE_NULL:
                val = NIL_VAL;
                break;
            default:
                val = NIL_VAL;
                break;
            }

            // set field on row instance
            ObjString *key = copyString(colName, (int)strlen(colName));
            push(OBJ_VAL(key));
            push(val);
            tableSet(&row->fields, key, val);
            pop(); // val
            pop(); // key
        }

        // add row to results array
        if (results->count == results->capacity)
        {
            int oldCap = results->capacity;
            results->capacity = GROW_CAPACITY(oldCap);
            results->values = GROW_ARRAY(Value, results->values, oldCap, results->capacity);
        }
        results->values[results->count++] = OBJ_VAL(row);
        pop(); // row
    }

    sqlite3_finalize(stmt);
    pop(); // results
    return OBJ_VAL(results);
}

static Value dbLastId(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;
    sqlite3 *db = getConnection(args[0]);
    if (db == NULL)
        return NIL_VAL;
    return NUMBER_VAL((double)sqlite3_last_insert_rowid(db));
}

static Value dbChanges(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;
    sqlite3 *db = getConnection(args[0]);
    if (db == NULL)
        return NIL_VAL;
    return NUMBER_VAL((double)sqlite3_changes(db));
}

static Value dbClose(int argCount, Value *args)
{
    if (argCount != 1)
        return NIL_VAL;

    sqlite3 *db = getConnection(args[0]);
    if (db == NULL)
    {
        printf("Error: invalid connection\n");
        return NIL_VAL;
    }

    sqlite3_close(db);

    // null out the pointer so it can't be used again
    if (IS_INSTANCE(args[0]))
    {
        ObjInstance *conn = AS_INSTANCE(args[0]);
        ObjString *key = copyString("__ptr", 5);
        push(OBJ_VAL(key));
        tableSet(&conn->fields, key, NUMBER_VAL(0));
        pop();
    }

    return BOOL_VAL(true);
}

ObjModule *initDbModule(void)
{
    ObjString *name = copyString("db", 2);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "open", dbOpen);
    setNative(module, "exec", dbExec);
    setNative(module, "query", dbQuery);
    setNative(module, "lastId", dbLastId);
    setNative(module, "changes", dbChanges);
    setNative(module, "close", dbClose);

    pop(); // module
    pop(); // name
    return module;
}
