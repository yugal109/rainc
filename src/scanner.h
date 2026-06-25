#ifndef rain_scanner_h
#define rain_scanner_h
typedef enum
{
    // Single-character tokens.
    TOKEN_LEFT_PAREN,    // 0
    TOKEN_RIGHT_PAREN,   // 1
    TOKEN_LEFT_BRACE,    // 2
    TOKEN_RIGHT_BRACE,   // 3
    TOKEN_LEFT_BRACKET,  // 4  [
    TOKEN_RIGHT_BRACKET, // 5  ]
    TOKEN_COMMA,         // 6
    TOKEN_DOT,           // 7
    TOKEN_MINUS,         // 8
    TOKEN_PLUS,          // 9
    TOKEN_SEMICOLON,     // 10
    TOKEN_SLASH,         // 11
    TOKEN_STAR,          // 12
                // One or two character tokens.
    TOKEN_BANG,          // 13
    TOKEN_BANG_EQUAL,    // 14
    TOKEN_EQUAL,         // 15
    TOKEN_EQUAL_EQUAL,   // 16
    TOKEN_GREATER,       // 17
    TOKEN_GREATER_EQUAL, // 18
    TOKEN_LESS,          // 19
    TOKEN_LESS_EQUAL,    // 20
    TOKEN_COLON_COLON,   // 21  ::
                       // Array tokens.
    TOKEN_HASH_LEFT_BRACKET, // 22  #[
                             // Literals.
    TOKEN_IDENTIFIER, // 23
    TOKEN_STRING,     // 24
    TOKEN_NUMBER,     // 25
                  // Keywords.
    TOKEN_AND,    // 26
    TOKEN_CLASS,  // 27
    TOKEN_ELSE,   // 28
    TOKEN_FALSE,  // 29
    TOKEN_FOR,    // 30
    TOKEN_FUN,    // 31
    TOKEN_IF,     // 32
    TOKEN_BENUTZEN, // 33
    TOKEN_NIL,    // 34
    TOKEN_OR,     // 35
    TOKEN_PRINT,  // 36
    TOKEN_RETURN, // 37
    TOKEN_SUPER,  // 38
    TOKEN_THIS,   // 39
    TOKEN_TRUE,   // 40
    TOKEN_VAR,    // 41
    TOKEN_WHILE,  // 42
    TOKEN_ARROW,  // 43  ->
    TOKEN_ERROR,  // 44
    TOKEN_EOF     // 45
} TokenType;
typedef struct
{
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;
void initScanner(const char *source);
Token scanToken();
#endif
