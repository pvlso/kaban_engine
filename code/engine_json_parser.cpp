/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal void
JsonAddToken(json_parser *Parser, json_token_type Type, char *Value)
{
    Parser->Tokens[Parser->TokenCount].Type = Type;
    Parser->Tokens[Parser->TokenCount].Value = PushString(Parser->TempArena, Value);
    ++Parser->TokenCount;    
}

inline b32
IsDigit(char C)
{
    b32 Result = ((C >= '0') && (C <= '9'));
    return(Result);
}

internal void
JsonTokenize(json_parser *Parser, char *Json)
{
    char *At = Json;
    u32 BufferIndex = 0;
    char Buffer[1024];

    while(*At)
    {
        while(IsWhitespace(*At))
        {
            ++At;
        }
        
        switch(*At)
        {
            case '{':
            {
                JsonAddToken(Parser, Token_CurlyOpen, "{");
                ++At;
            } break;

            case '}':
            {
                JsonAddToken(Parser, Token_CurlyClose, "}");
                ++At;
            } break;

            case '[':
            {
                JsonAddToken(Parser, Token_SquareOpen, "[");
                ++At;
            } break;

            case ']':
            {
                JsonAddToken(Parser, Token_SquareClose, "]");
                ++At;
            } break;

            case ':':
            {
                JsonAddToken(Parser, Token_Colon, ":");
                ++At;
            } break;

            case ',':
            {
                JsonAddToken(Parser, Token_Comma, ",");
                ++At;
            } break;

            case '"':
            {
                ++At;
                BufferIndex = 0;
                while(*At && (*At != '"'))
                {
                    Buffer[BufferIndex++] = *At++;
                }

                Buffer[BufferIndex] = 0;
                JsonAddToken(Parser, Token_String, Buffer);
                ++At;
            } break;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                BufferIndex = 0;
                while(IsDigit(*At) || (*At == '.') || (*At == 'e') ||
                      (*At == 'E') || (*At == '-') || (*At == '+'))
                {
                    Buffer[BufferIndex++] = *At++;
                }
                Buffer[BufferIndex] = 0;
                JsonAddToken(Parser, Token_Number, Buffer);
            } break;

            case 'f':
            {
                JsonAddToken(Parser, Token_False, "false");
                At += 5;
            } break;

            case 't':
            {
                JsonAddToken(Parser, Token_True, "true");
                At += 4;
            } break;

            case 'n':
            {
                JsonAddToken(Parser, Token_Null, "null");
                At += 4;
            } break;
        }
    }
}

inline json_token
GetNextToken(json_parser *Parser)
{
    json_token Result = {};
    if(Parser->CurrentTokenIndex < Parser->TokenCount)
    {
        Result = Parser->Tokens[Parser->CurrentTokenIndex];
        Parser->CurrentTokenIndex += 1;
    }

    return(Result);
}

inline json_value *
JsonParsePair(json_parser *Parser)
{
    json_value *Value = 0;
    json_token Token = GetNextToken(Parser);
    if(Token.Type == Token_Colon)
    {
        Token = GetNextToken(Parser);
        Value = JsonParseToken(Parser, Token);
    }
    else
    {
        Assert(!"Expect Colon");
    }

    return(Value);
}

inline void
JsonParseObject(json_parser *Parser, json_value *Value)
{
    Value->Object.Pairs = PushArray(Parser->TempArena, 1024, json_pair *);
    json_token Token = GetNextToken(Parser);
    while(Token.Type != Token_CurlyClose)
    {
        switch(Token.Type)
        {
            case Token_String:
            {
                Value->Object.Pairs[Value->Object.Count] = PushStruct(Parser->TempArena, json_pair);
                Value->Object.Pairs[Value->Object.Count]->Key = Token.Value;
                Value->Object.Pairs[Value->Object.Count]->Value = JsonParsePair(Parser);
                ++Value->Object.Count;
            } break;

            case Token_Comma:
                break;

            default:
            {
                Assert(!"Expect String");
            } break;
        }

        Token = GetNextToken(Parser);
    }
}

inline void
JsonParseArray(json_parser *Parser, json_value *Value)
{
    Value->Array.Items = PushArray(Parser->TempArena, 1024, json_value *);
    json_token Token = GetNextToken(Parser);
    while(Token.Type != Token_SquareClose)
    {
        switch(Token.Type)
        {
            case Token_Comma:
                break;

            default:
            {
                Value->Array.Items[Value->Array.Count] = PushStruct(Parser->TempArena, json_value);
                Value->Array.Items[Value->Object.Count] = JsonParseToken(Parser, Token);
                ++Value->Object.Count;
            } break;
        }

        Token = GetNextToken(Parser);
    }
}

inline s64
ParseInt(char *At)
{
    s32 Neg = 0;
    if(*At == '-')
    {
        Neg = 1;
    }
    else if(*At == '+')
        At++;

    s64 Result = 0;
    while((*At >= '0') && (*At <= '9'))
    {
        Result = Result * 10 + (*At - '0');
        At++;
    }

    return(Neg ? -Result : Result);
}

inline f64
ParseFloat(char *At)
{
    s32 Neg = 0;
    if(*At == '-')
    {
        Neg = 1;
        At++;
    }
    else if(*At == '+')
        At++;

    f64 Result = 0.0;
    while((*At >= '0') && (*At <= '9'))
    {
        Result = Result * 10.0 + (*At - '0');
        At++;
    }

    if(*At == '.')
    {
        At++;
        f64 Frac = 0.1;
        while((*At >= '0') && (*At <= '9'))
        {
            Result += (*At - '0') * Frac;
            Frac *= 0.1;
            At++;
        }
    }

    if(*At == 'e' || *At == 'E')
    {
        At++;
        s32 ExpNeg = 0;
        if(*At == '-')
        {
            ExpNeg = 1;
            At++;
        }
        else if(*At == '+')
            At++;

        s32 Exp = 0;
        while((*At >= '0') && (*At <= '9'))
        {
            Exp = Exp * 10 + (*At - '0');
            At++;
        }

        f64 Pow10 = 1.0;
        while(Exp--)
            Pow10 *= 10.0;

        Result = ExpNeg ? (Result / Pow10) : (Result * Pow10);
    }

    return(Neg ? -Result : Result);
}


internal json_value *
JsonParseToken(json_parser *Parser, json_token Token)
{
    json_value *Value = PushStruct(Parser->TempArena, json_value);
    switch(Token.Type)
    {
        case Token_CurlyOpen:
        {
            Value->Type = JsonValue_Object;
            JsonParseObject(Parser, Value);
        } break;

        case Token_SquareOpen:
        {
            Value->Type = JsonValue_Array;
            JsonParseArray(Parser, Value);
        } break;

        case Token_String:
        {
            Value->Type = JsonValue_String;
            Value->String = Token.Value;
        } break;

        case Token_Number:
        {
            char *At = Token.Value;
            while((*At != 0) && (*At != '.') &&
                  (*At != 'e') && (*At != 'E'))
                At++;

            if((*At == '.') || (*At == 'e') || (*At == 'E'))
            {
                Value->Type = JsonValue_Double;
                Value->Double = ParseFloat(Token.Value);
            }
            else
            {
                Value->Type = JsonValue_Int;
                Value->Int = ParseInt(Token.Value);
            }
        } break;

        case Token_True:
        {
            Value->Type = JsonValue_Bool;
            Value->Bool = true;
        } break;

        case Token_False:
        {
            Value->Type = JsonValue_Bool;
            Value->Bool = false;
        } break;

        case Token_Null:
        {
            Value->Type = JsonValue_Null;
        } break;


        default:
        {
            Assert(!"Unexpected Token");
        } break;
    }

    return(Value);
}


internal json_value *
JsonLookupObjectElement(json_object *Object, char *Key)
{
    json_value *Result = 0;
    if(Object)
    {
        for(u32 I = 0; I < Object->Count; ++I)
        {
            json_pair *Pair = Object->Pairs[I];
            if(StringsAreEqual(Key, Pair->Key))
            {
                Result = Pair->Value;
                break;
            }
        }
    }

    return(Result);
}

inline char *
JsonGetEnumString(json_object *Head, char *Key, u32 Index)
{
    char *Result = 0;

    json_value *EnumStrings = JsonLookupObjectElement(Head, Key);
    if(EnumStrings)
    {
        Assert(EnumStrings->Type == JsonValue_Array);
        Assert(EnumStrings->Array.Items[Index]->Type == JsonValue_String);
        Result = EnumStrings->Array.Items[Index]->String;
    }

    return(Result);
}

inline char *
JsonGetTagValueEnumKey(json_object *Head, u32 TagValue)
{
    char *Result = 0;

    json_value *TagTable = JsonLookupObjectElement(Head, "TagValuesTable");
    if(TagTable)
    {
        char *TagString = JsonGetEnumString(Head, "AssetTag", TagValue);

        Assert(TagTable->Type == JsonValue_Object);
        json_value *EnumKey = JsonLookupObjectElement(&TagTable->Object, TagString);

        Assert(EnumKey->Type == JsonValue_String);
        Result = EnumKey->String;
    }
    
    return(Result);
}

internal void
PrintTokens(json_parser *Parser)
{
    char Buffer[1024];
    FILE *File;
    fopen_s(&File, "json_parsed_tokens.txt", "wb");
    for(u32 TokenIndex = 0;
        TokenIndex < Parser->TokenCount;
        ++TokenIndex)
    {
        json_token *Token = Parser->Tokens + TokenIndex;
        u32 Length = (u32)FormatString(ArrayCount(Buffer), Buffer, "Type: %d, Value: %s\n", Token->Type, Token->Value);
        fwrite(Buffer, Length, 1, File);
    }
    fclose(File);
}

internal json_object *
ParseJson(char *FileName, memory_arena *Arena)
{
    json_object *Head = 0;

    json_parser JsonParser = {};
    JsonParser.TempArena = Arena;
    JsonParser.CurrentTokenIndex = 1;
    JsonParser.Tokens = PushArray(Arena, 4096, json_token);

    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_JSON, 0);
    if(ReadResult.Contents)
    {
        JsonTokenize(&JsonParser, (char *)ReadResult.Contents);

#if EDITOR_INTERNAL
        PrintTokens(&JsonParser);
#endif

        json_value *Result = PushStruct(Arena, json_value);
        Result->Type = JsonValue_Object;
        JsonParseObject(&JsonParser, Result);

        Head = &Result->Object;
    }

    Platform.FreeFileMemory(ReadResult.Contents);

    return(Head);
}
