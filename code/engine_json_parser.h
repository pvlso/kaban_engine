#if !defined(EDITOR_JSON_PARSER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum json_token_type
{
    Token_None,

    Token_CurlyOpen,
    Token_CurlyClose,
    Token_SquareOpen,
    Token_SquareClose,
    Token_Colon,
    Token_Comma,
    Token_String,
    Token_Number,
    Token_True,
    Token_False,
    Token_Null,

    Token_Count,
};

struct json_token
{
    json_token_type Type;
    char *Value;
};

enum json_value_type
{
    JsonValue_Null,
    JsonValue_Bool,
    JsonValue_Int,
    JsonValue_Double,
    JsonValue_String,
    JsonValue_Array,
    JsonValue_Object,
    
    JsonValue_Count,
};

struct json_pair;
struct json_value;
struct json_array
{
    u32 Count;
    json_value **Items;
};

struct json_object
{
    u32 Count;
    json_pair **Pairs;
};

struct json_value
{
    json_value_type Type;
    union
    {
        b32 Bool;
        s64 Int;
        f64 Double;
        char *String;
        json_array Array;
        json_object Object;
    };
};

struct json_pair
{
    char *Key;
    json_value *Value;
};

struct json_parser
{
    memory_arena *TempArena;

    u32 CurrentTokenIndex;
    u32 TokenCount;
    json_token *Tokens;
};

internal json_value *JsonParseToken(json_parser *Parser, json_token Token);

#define EDITOR_JSON_PARSER_H
#endif
