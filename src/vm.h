#ifndef rain_vm_h
#define rain_vm_h

#include "chunk.h" // VM executes this Chunk
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjClosure *closure; // which fuction is running
    uint8_t *ip;         // where we are in its bytecode
    Value *slots;        // where its locals live on stack
} CallFrame;

typedef enum
{
    INTERPRET_OK,            // execution succeeded
    INTERPRET_COMPILE_ERROR, // compiler found a static error
    INTERPRET_RUNTIME_ERROR  // VM hit an error at runtime
} InterpretResult;

typedef struct
{
    CallFrame frames[FRAMES_MAX]; // call stack
    int frameCount;               // number of active frames

    Value stack[STACK_MAX]; // the value stack - fixed size array lives inside VM struct
    Value *stackTop;

    Table strings;
    ObjString *initString;
    Table globals;
    ObjUpvalue *openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;

    Obj *objects;
    int grayCount;
    int grayCapacity;
    Obj **grayStack;
} VM;

extern VM vm;

void initVM();                                 // initialize the VM
void freeVM();                                 // free the VM
InterpretResult interpret(const char *source); // takes in source string, not chunk
void push(Value value);                        // push a value onto the stack
Value pop();                                   // pop the top value off the stack

#endif
