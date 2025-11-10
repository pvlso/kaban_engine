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
                JsonAddToken(Parser, Token_False, "true");
                At += 4;
            } break;

            case 'n':
            {
                JsonAddToken(Parser, Token_False, "null");
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

internal json_element *
JsonParseList(json_parser *Parser, json_token StartingToken, json_token_type EndType, b32 HasLabels);

internal json_element *
JsonParseElement(json_parser *Parser, char *Label, json_token Value)
{
    b32 Valid = true;
    json_element *SubElement = 0;
    if(Value.Type == Token_CurlyOpen)
    {
        SubElement = JsonParseList(Parser, Value, Token_CurlyClose, true);
    }
    else if(Value.Type == Token_SquareOpen)
    {
        SubElement = JsonParseList(Parser, Value, Token_SquareClose, false);
    }
    else if((Value.Type == Token_String) ||
            (Value.Type == Token_True) ||
            (Value.Type == Token_False) ||
            (Value.Type == Token_Null) ||
            (Value.Type == Token_Number))
    {
    }
    else
    {
        Valid = false;
    }

    json_element *Result = 0;
    if(Valid)
    {
        Result = PushStruct(Parser->TempArena, json_element);
        Result->Label = Label;
        Result->Value = Value.Value;
        Result->FirstSubElement = SubElement;
        Result->NextSibling = 0;
    }

    return(Result);
}

internal json_element *
JsonParseList(json_parser *Parser, json_token StartingToken, json_token_type EndType, b32 HasLabels)
{
    json_element *FirstElement = 0;
    json_element *LastElement = 0;

    while(1)
    {
        char *Label = 0; 
        json_token Value = GetNextToken(Parser);
        if(HasLabels)
        {
            if(Value.Type == Token_String)
            {
                Label = Value.Value;

                json_token Colon = GetNextToken(Parser);
                if(Colon.Type == Token_Colon)
                {
                    Value = GetNextToken(Parser);
                }
                else
                {
                    Assert(!"Colon Expected");
                    FirstElement = 0;
                    break;
                }
            }
            else if(Value.Type != EndType)
            {
                Assert(!"UnExpected token");
                FirstElement = 0;
                break;
            }
        }

        json_element *Element = JsonParseElement(Parser, Label, Value);
        if(Element)
        {
            LastElement = (LastElement ? LastElement->NextSibling : FirstElement) = Element;
        }
        else if(Value.Type == EndType)
        {
            break;
        }
        else
        {
            Assert(!"UnExpected token");
            FirstElement = 0;
            break;
        }

        json_token Comma = GetNextToken(Parser);
        if(Comma.Type == EndType)
        {
            break;
        }
        else if(Comma.Type != Token_Comma)
        {
            Assert(!"Expected comma or endtype");
            FirstElement = 0;
            break;
        }
    }

    return(FirstElement);
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

internal json_element *
JsonLookupElement(json_element *Object, char *Key)
{
    json_element *Result = 0;
    if(Object)
    {
        for(json_element *Search = Object->FirstSubElement;
            Search;
            Search = Search->NextSibling)
        {
            if(StringsAreEqual(Key, Search->Label))
            {
                Result = Search;
                break;
            }
        }
    }

    Assert(Result);

    return(Result);
}

inline char *
JsonGetEnumString(json_element *Head, char *Key, u32 Index)
{
    char *Result = 0;

    json_element *EnumStrings = JsonLookupElement(Head, Key);
    if(EnumStrings)
    {
        json_element *EnumString = EnumStrings->FirstSubElement;
        for(u32 ElementIndex = 0;
            ElementIndex < Index;
            ++ElementIndex)
        {
            EnumString = EnumString->NextSibling;
        }

        Result = EnumString->Value;
    }

    Assert(Result);

    return(Result);
}

inline char *
JsonGetTagValueEnumKey(json_element *Head, u32 TagValue)
{
    char *Result = 0;

    json_element *TagTable = JsonLookupElement(Head, "TagValuesTable");
    char *TagString = JsonGetEnumString(Head, "AssetTag", TagValue);
    json_element *EnumKey = JsonLookupElement(TagTable, TagString);

    Result = EnumKey->Value;

    Assert(Result);
    
    return(Result);
}

internal json_element *
ParseJson(char *FileName, memory_arena *Arena)
{
    json_element *Head = 0;

    json_parser JsonParser = {};
    JsonParser.TempArena = Arena;
    JsonParser.Tokens = PushArray(Arena, 4096, json_token);

    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_JSON, 0);
    if(ReadResult.Contents)
    {
        JsonTokenize(&JsonParser, (char *)ReadResult.Contents);

#if EDITOR_INTERNAL
        PrintTokens(&JsonParser);
#endif

        u32 TokenIndex = 0;
        Head = JsonParseElement(&JsonParser, 0, GetNextToken(&JsonParser));
    }

    Platform.FreeFileMemory(ReadResult.Contents);

    return(Head);
}
