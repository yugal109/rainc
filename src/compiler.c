#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
    Token current;  // token we are looking right now
    Token previous; // token that we just consumed
    bool hadError;  // has any error occurred?
    bool panicMode; // are we in panic mode ?
} Parser;

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // ==
    PREC_COMPARISION, // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct
{
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler
{
    struct Compiler *enclosing;

    ObjFunction *function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;

} Compiler;

typedef struct ClassCompiler
{
    struct ClassCompiler *enclosing;
    Token name;
    bool hasSuperclass;
} ClassCompiler;

// forward declarations
static void binary(bool canAssign);
static void unary(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);
static void grouping(bool canAssign);
static void array(bool canAssign);
static void arrayIndex(bool canAssign);
static void arrow(bool canAssign);
static void expression(void);
static void statement();
static void declaration();
static void forStatement(void);
static void expressionStatement();
static void parsePrecedence(Precedence);
static bool identifiersEqual(Token *, Token *);
static void defineVariable(uint16_t);
static void declareVariable(void);
static void markInitialized(void);
static void variable(bool);
static uint16_t identifierConstant(Token *);
static uint8_t argumentList(void);
static void ifStatement(void);
static uint16_t parseVariable(const char *);
static void addLocal(Token);
static Token syntheticToken(const char *);
static ParseRule *getRule(TokenType);
static ObjFunction *endCompiler(void);
static void emitReturn(void);
static void namedVariable(Token, bool);
static void moduleAccess(bool canAssign);
static void importStatement(void);

Parser parser; // single global instance

Compiler *current = NULL;

Chunk *compilingChunk;

ClassCompiler *currentClass = NULL;

static Chunk *currentChunk()
{
    return &current->function->chunk;
}

static void
errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;              // supress cascade errors
    parser.panicMode = true; // enter panic mode

    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // NOTHING
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;
    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1); // emit opcode
    emitByte(byte2); // emit operand
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn()
{
    if (current->type == TYPE_INITIALIZER)
    {
        emitBytes(OP_GET_LOCAL, 0);
    }
    else
    {
        emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static void emitIndex(uint16_t index)
{
    emitByte((index >> 8) & 0xff);
    emitByte(index & 0xff);
}

static uint16_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT16_MAX)
    {
        error("Too many constants in one chunk");
        return 0;
    }
    return (uint16_t)constant;
}

static void emitConstant(Value value)
{
    emitByte(OP_CONSTANT);
    emitIndex(makeConstant(value));
}

static void patchJump(int offset)
{
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX)
    {
        error("too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION)
    {
        local->name.start = "this";
        local->name.length = 4;
    }
    else
    {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction *endCompiler()
{
    emitReturn();
    ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        if (current->locals[current->localCount - 1].isCaptured)
        {

            emitByte(OP_CLOSE_UPVALUE);
        }
        else
        {

            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

static void binary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    // Compile the right operand.
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType)
    {
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
        break;
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    default:
        return; // Unreachable.
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void dot(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.' .");
    uint16_t name = identifierConstant(&parser.previous);
    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitByte(OP_SET_PROPERTY);
        emitIndex(name);
    }
    else if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        emitByte(OP_INVOKE);
        emitIndex(name);
        emitByte(argCount);
    }
    else
    {

        emitByte(OP_GET_PROPERTY);
        emitIndex(name);
    }
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint16_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters."); // ← ADD THIS
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    // create the function object
    ObjFunction *function = endCompiler();
    emitByte(OP_CLOSURE);
    emitIndex(makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++)
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method()
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint16_t constant = identifierConstant(&parser.previous);
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0)
    {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitByte(OP_METHOD);
    emitIndex(constant);
}

static uint16_t identifierConstant(Token *name)
{
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void classDeclaration()
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint16_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitByte(OP_CLASS);
    emitIndex(nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.name = parser.previous;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS))
    {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);
        if (identifiersEqual(&className, &parser.previous))
        {
            error("A class can't inherit from itself.");
        }

        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass)
    {
        endScope();
    }

    currentClass = currentClass->enclosing;
}

static void funDeclaration()
{
    uint16_t global = parseVariable("expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static bool identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }

            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;

    // check if it's already there in the upvalue or not
    for (int i = 0; i < upvalueCount; i++)
    {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
        {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name)
{
    if (compiler->enclosing == NULL)
        return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
    {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
    {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }
    Local *local = &current->locals[current->localCount++];
    local->name = name;

    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable()
{
    if (current->scopeDepth == 0)
        return;
    Token *name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            error("Already variable with this name in this scope.");
        }
    }
    addLocal(*name);
}

static uint16_t parseVariable(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();           // declare local if in block
    if (current->scopeDepth > 0) // locals don't need constant index
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint16_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }
    emitByte(OP_DEFINE_GLOBAL);
    emitIndex(global);
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();
            if (argCount == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void varDeclaration()
{
    uint16_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
}

static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        if (current->type == TYPE_INITIALIZER)
        {
            error("can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement()
{
    int loopstart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'while'.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    statement();

    emitLoop(loopstart);

    patchJump(exitJump);

    emitByte(OP_POP);
}

static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();

    patchJump(elseJump);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        default:
            // do nothing
            ;
        }
        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_BENUTZEN))
    {
        importStatement();
    }
    else if (match(TOKEN_CLASS))
    {
        classDeclaration();
    }
    else if (match(TOKEN_FUN))
    {
        funDeclaration();
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        statement();
    }
    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else
    {
        expressionStatement();
    }
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression");
    emitByte(OP_POP);
}

static void forStatement()
{
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // initializer
    if (match(TOKEN_SEMICOLON))
    {
        // no initializer --  do nothing
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;

    // condition clause

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;

        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL); // convert lexeme to double
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign)
{
    const char *src = parser.previous.start + 1;
    int len = parser.previous.length - 2;

    char *buffer = (char *)malloc(len + 1);
    int outLen = 0;

    for (int i = 0; i < len; i++)
    {
        if (src[i] == '\\' && i + 1 < len)
        {
            i++;
            switch (src[i])
            {
            case 'n':
                buffer[outLen++] = '\n';
                break;
            case 't':
                buffer[outLen++] = '\t';
                break;
            case 'r':
                buffer[outLen++] = '\r';
                break;
            case '\\':
                buffer[outLen++] = '\\';
                break;
            case '"':
                buffer[outLen++] = '"';
                break;
            case '0':
                buffer[outLen++] = '\0';
                break;
            default:
                buffer[outLen++] = '\\';
                buffer[outLen++] = src[i];
                break;
            }
        }
        else
        {
            buffer[outLen++] = src[i];
        }
    }

    ObjString *str = copyString(buffer, outLen);
    free(buffer);

    // push onto Rain stack for GC safety before emitConstant
    push(OBJ_VAL(str));
    emitConstant(OBJ_VAL(str));
    pop();
}
static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);

    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1)
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        if (setOp == OP_SET_GLOBAL)
        {
            emitByte(setOp);
            emitIndex((uint16_t)arg);
        }
        else
        {
            emitBytes(setOp, (uint8_t)arg);
        }
    }
    else
    {
        if (getOp == OP_GET_GLOBAL)
        {
            emitByte(getOp);
            emitIndex((uint16_t)arg);
        }
        else
        {
            emitBytes(getOp, (uint8_t)arg);
        }
    }
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char *text)
{
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign)
{
    if (currentClass == NULL)
    {
        error("can't user 'super' outside of a class.");
    }
    else if (!currentClass->hasSuperclass)
    {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint16_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);

    if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitByte(OP_SUPER_INVOKE);
        emitIndex(name);
        emitByte(argCount);
    }
    else
    {
        namedVariable(syntheticToken("super"), false);
        emitByte(OP_GET_SUPER);
        emitIndex(name);
    }
}

static void this_(bool canAssign)
{
    if (currentClass == NULL)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType)
    {

    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
    {
        emitByte(OP_NEGATE);
        break;
    }
    default:
        return;
    }
}

static void array(bool canAssign)
{
    int elementCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET))
    {
        do
        {
            expression();
            if (elementCount == 255)
            {
                error("Can't have more than 255 elements in array literal.");
            }
            elementCount++;

        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");
    emitBytes(OP_ARRAY_BUILD, (uint8_t)elementCount);
}

static void arrayIndex(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitByte(OP_INDEX_SET);
    }
    else
    {
        emitByte(OP_INDEX_GET);
    }
}

static void arrow(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "expect method name after '->'.");
    uint16_t name = identifierConstant(&parser.previous);
    if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        emitByte(OP_INVOKE);
        emitIndex(name);
        emitByte(argCount);
    }
    else
    {
        emitByte(OP_GET_PROPERTY);
        emitIndex(name);
    }
}

static void moduleAccess(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect field name after '::'.");
    uint16_t name = identifierConstant(&parser.previous);

    if (match(TOKEN_LEFT_PAREN))
    {
        emitByte(OP_GET_MODULE);
        emitIndex(name);
        uint8_t argCount = argumentList();
        emitBytes(OP_CALL, argCount);
    }
    else
    {
        emitByte(OP_GET_MODULE);
        emitIndex(name);
    }
}

static void importStatement()
{
    consume(TOKEN_STRING, "Expect module name after 'benutzen'.");

    // full path string e.g. "math" or "utils.rn"
    int fullLength = parser.previous.length - 2;
    const char *fullName = parser.previous.start + 1;

    // compute stem name - strip .rn if present
    int stemLength = fullLength;
    if (fullLength > 3 &&
        fullName[fullLength - 3] == '.' &&
        fullName[fullLength - 2] == 'r' &&
        fullName[fullLength - 1] == 'n')
    {
        stemLength = fullLength - 3;
    }

    // also strip directory prefix for stem
    int stemStart = 0;
    for (int i = stemLength - 1; i >= 0; i--)
    {
        if (fullName[i] == '/')
        {
            stemStart = i + 1;
            break;
        }
    }

    // push __import__ function onto stack
    Token importToken = syntheticToken("__import__");
    namedVariable(importToken, false);

    // push full path as argument
    emitConstant(OBJ_VAL(copyString(fullName, fullLength)));

    emitBytes(OP_CALL, 1);

    // define global variable with stem name
    // "math" → var math = ...
    // "utils.rn" → var utils = ...
    // "models/nn.rn" → var nn = ...
    uint16_t global = makeConstant(OBJ_VAL(copyString(fullName + stemStart, stemLength - stemStart)));
    emitByte(OP_DEFINE_GLOBAL);
    emitIndex(global);

    consume(TOKEN_SEMICOLON, "Expect ';' after module name.");
}

static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISION},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISION},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISION},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISION},
    [TOKEN_LEFT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_HASH_LEFT_BRACKET] = {array, arrayIndex, PREC_CALL},
    [TOKEN_ARROW] = {NULL, arrow, PREC_CALL},
    [TOKEN_COLON_COLON] = {NULL, moduleAccess, PREC_CALL},
    [TOKEN_BENUTZEN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL)
    {
        error("Expect expression");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target");
    }
}

ObjFunction *compile(const char *source)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }
    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots()
{
    Compiler *compiler = current;
    while (compiler != NULL)
    {
        markObject((Obj *)compiler->function);
        compiler = compiler->enclosing;
    }
}
