#ifndef rain_scanner_h
#define rain_scanner_h
typedef enum
{
    // Single-character tokens.
    TOKEN_LEFT_PAREN,        // 0
    TOKEN_RIGHT_PAREN,       // 1
    TOKEN_LEFT_BRACE,        // 2
    TOKEN_RIGHT_BRACE,       // 3
    TOKEN_LEFT_BRACKET,      // 4  [
    TOKEN_RIGHT_BRACKET,     // 5  ]
    TOKEN_COMMA,             // 6
    TOKEN_DOT,               // 7
    TOKEN_MINUS,             // 8
    TOKEN_PLUS,              // 9
    TOKEN_SEMICOLON,         // 10
    TOKEN_SLASH,             // 11
    TOKEN_STAR,              // 12
                             // One or two character tokens.
    TOKEN_BANG,              // 13
    TOKEN_BANG_EQUAL,        // 14
    TOKEN_EQUAL,             // 15
    TOKEN_EQUAL_EQUAL,       // 16
    TOKEN_GREATER,           // 17
    TOKEN_GREATER_EQUAL,     // 18
    TOKEN_LESS,              // 19
    TOKEN_LESS_EQUAL,        // 20
                             // Array tokens.
    TOKEN_HASH_LEFT_BRACKET, // 21  #[
                             // Literals.
    TOKEN_IDENTIFIER,        // 22
    TOKEN_STRING,            // 23
    TOKEN_NUMBER,            // 24
                             // Keywords.
    TOKEN_AND,               // 25
    TOKEN_CLASS,             // 26
    TOKEN_ELSE,              // 27
    TOKEN_FALSE,             // 28
    TOKEN_FOR,               // 29
    TOKEN_FUN,               // 30
    TOKEN_IF,                // 31
    TOKEN_NIL,               // 32
    TOKEN_OR,                // 33
    TOKEN_PRINT,             // 34
    TOKEN_RETURN,            // 35
    TOKEN_SUPER,             // 36
    TOKEN_THIS,              // 37
    TOKEN_TRUE,              // 38
    TOKEN_VAR,               // 39
    TOKEN_WHILE,             // 40
    TOKEN_ARROW,             // 41  ->
    TOKEN_ERROR,             // 42
    TOKEN_EOF                // 43
} TokenType;
typedef struct
{
    TokenType type;    // what kind of token
    const char *start; // pointer into source string - no copying
    int length;        // how many chars in lexeme
    int line;          // source line for error reporting
} Token;
void initScanner(const char *source);
Token scanToken();
#endif
