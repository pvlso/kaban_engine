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

struct json_element
{
    char *Label;
    char *Value;
    json_element *FirstSubElement;

    json_element *NextSibling;
};

struct json_parser
{
    memory_arena *TempArena;

    u32 CurrentTokenIndex;
    u32 TokenCount;
    json_token *Tokens;
};

#define EDITOR_JSON_PARSER_H
#endif
