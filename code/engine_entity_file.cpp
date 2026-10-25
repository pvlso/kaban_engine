/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): Entities' types and entity files, the text formats: see engine_entity_file.h. Lines are read with the
// tile sets' tokenizer (engine_tileset_file.cpp).

#define ENTITY_TYPES_FILE_NAME "entity_types.txt"

global_variable char *EntityFieldKindNames[EntityField_Count] = {"int", "number", "bool", "text"};

internal s32
FindEntityType(entity_types *Types, char *Name)
{
    s32 Result = -1;
    for(u32 TypeIndex = 0; TypeIndex < Types->Count; ++TypeIndex)
    {
        if(StringsAreEqual(Types->Types[TypeIndex].Name, Name)) {Result = (s32)TypeIndex;}
    }

    return(Result);
}

internal s32
FindEntityState(entity_type *Type, char *Name)
{
    s32 Result = -1;
    for(u32 StateIndex = 0; StateIndex < Type->StateCount; ++StateIndex)
    {
        if(StringsAreEqual(Type->States[StateIndex], Name)) {Result = (s32)StateIndex;}
    }

    return(Result);
}

internal s32
FindEntityField(entity_type *Type, char *Name)
{
    s32 Result = -1;
    for(u32 FieldIndex = 0; FieldIndex < Type->FieldCount; ++FieldIndex)
    {
        if(StringsAreEqual(Type->Fields[FieldIndex].Name, Name)) {Result = (s32)FieldIndex;}
    }

    return(Result);
}

// NOTE(pvlso): False when Token isn't a value of that kind
internal b32
ReadEntityFieldValue(entity_field_kind Kind, char *Token, entity_field_value *Value)
{
    b32 Result = true;
    switch(Kind)
    {
        case EntityField_Int:
        {
            Result = IsTilesetNumber(Token);
            if(Result) {Value->Int = RoundReal32ToInt32(R32FromTilesetToken(Token));}
        } break;

        case EntityField_Number:
        {
            Result = IsTilesetNumber(Token);
            if(Result) {Value->Number = R32FromTilesetToken(Token);}
        } break;

        case EntityField_Bool:
        {
            if(StringsAreEqual(Token, "yes") || StringsAreEqual(Token, "true") || StringsAreEqual(Token, "1"))
            {
                Value->Bool = true;
            }
            else if(StringsAreEqual(Token, "no") || StringsAreEqual(Token, "false") || StringsAreEqual(Token, "0"))
            {
                Value->Bool = false;
            }
            else
            {
                Result = false;
            }
        } break;

        case EntityField_Text:
        {
            FormatString(ENTITY_TEXT_LENGTH, Value->Text, "%s", Token);
        } break;

        InvalidDefaultCase;
    }

    return(Result);
}

// NOTE(pvlso): The project's data/txts/entity_types.txt, on Arena. Without it the project has no types.
internal entity_types *
ReadEntityTypes(memory_arena *Arena)
{
    entity_types *Result = PushStruct(Arena, entity_types);
    if(!Platform.FileExists(ENTITY_TYPES_FILE_NAME, PlatformFileType_TXT))
    {
        LOG_WARNING("No data/txts/%s, the project has no entity types", ENTITY_TYPES_FILE_NAME);
        return(Result);
    }

    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    read_file_result File = Platform.ReadEntireFile(ENTITY_TYPES_FILE_NAME, PlatformFileType_TXT, Arena, true);
    tileset_line_tokens *Tokens = PushStruct(Arena, tileset_line_tokens);
    char *At = (char *)File.Contents;
    char *FileEnd = At ? At + StringLength(At) : 0; // NOTE(pvlso): a text file's Size counts its closing zero
    u32 LineNumber = 0;
    entity_type *Type = 0;
    while(At && (At < FileEnd))
    {
        char *LineEnd = At;
        while((LineEnd < FileEnd) && (*LineEnd != '\n')) {++LineEnd;}
        ++LineNumber;
        TokenizeTilesetLine(At, LineEnd, Tokens);
        At = (LineEnd < FileEnd) ? LineEnd + 1 : LineEnd;
        if(!Tokens->Count)
        {
            continue;
        }

        char *Key = Tokens->Tokens[0];
        if(StringsAreEqual(Key, "type") && (Tokens->Count >= 2))
        {
            Type = 0;
            if(FindEntityType(Result, Tokens->Tokens[1]) >= 0)
            {
                LOG_ERROR("%s, line %d: the type %s again", ENTITY_TYPES_FILE_NAME, LineNumber, Tokens->Tokens[1]);
            }
            else if(Result->Count < ENTITY_MAX_TYPES)
            {
                Type = Result->Types + Result->Count++;
                FormatString(ENTITY_NAME_LENGTH, Type->Name, "%s", Tokens->Tokens[1]);
            }
            else
            {
                LOG_ERROR("%s, line %d: more than %d types", ENTITY_TYPES_FILE_NAME, LineNumber, ENTITY_MAX_TYPES);
            }
        }
        else if(!Type)
        {
            LOG_ERROR("%s, line %d: %s isn't under a type", ENTITY_TYPES_FILE_NAME, LineNumber, Key);
        }
        else if(StringsAreEqual(Key, "state") && (Tokens->Count >= 2))
        {
            if(Type->StateCount < ENTITY_MAX_STATES)
            {
                FormatString(ENTITY_NAME_LENGTH, Type->States[Type->StateCount++], "%s", Tokens->Tokens[1]);
            }
            else
            {
                LOG_ERROR("%s, line %d: %s has more than %d states", ENTITY_TYPES_FILE_NAME, LineNumber, Type->Name,
                          ENTITY_MAX_STATES);
            }
        }
        else if(StringsAreEqual(Key, "field") && (Tokens->Count >= 3))
        {
            s32 Kind = -1;
            for(u32 KindIndex = 0; KindIndex < EntityField_Count; ++KindIndex)
            {
                if(StringsAreEqual(Tokens->Tokens[2], EntityFieldKindNames[KindIndex])) {Kind = (s32)KindIndex;}
            }
            if(Kind < 0)
            {
                LOG_ERROR("%s, line %d: %s isn't a field kind (int, number, bool, text)", ENTITY_TYPES_FILE_NAME,
                          LineNumber, Tokens->Tokens[2]);
            }
            else if(Type->FieldCount >= ENTITY_MAX_FIELDS)
            {
                LOG_ERROR("%s, line %d: %s has more than %d fields", ENTITY_TYPES_FILE_NAME, LineNumber, Type->Name,
                          ENTITY_MAX_FIELDS);
            }
            else
            {
                entity_type_field *Field = Type->Fields + Type->FieldCount++;
                FormatString(ENTITY_NAME_LENGTH, Field->Name, "%s", Tokens->Tokens[1]);
                Field->Kind = (entity_field_kind)Kind;
                if((Tokens->Count >= 4) && !ReadEntityFieldValue(Field->Kind, Tokens->Tokens[3], &Field->Default))
                {
                    LOG_ERROR("%s, line %d: %s isn't a %s value", ENTITY_TYPES_FILE_NAME, LineNumber, Tokens->Tokens[3],
                              EntityFieldKindNames[Kind]);
                }
            }
        }
        else
        {
            LOG_ERROR("%s, line %d: can't read %s", ENTITY_TYPES_FILE_NAME, LineNumber, Key);
        }
    }
    EndTemporaryMemory(TempMem);

    // NOTE(pvlso): Every type has a state to be in
    for(u32 TypeIndex = 0; TypeIndex < Result->Count; ++TypeIndex)
    {
        entity_type *Each = Result->Types + TypeIndex;
        if(!Each->StateCount)
        {
            FormatString(ENTITY_NAME_LENGTH, Each->States[Each->StateCount++], "Normal");
        }
    }
    Result->Loaded = true;
    LOG("Read %s: %u entity types", ENTITY_TYPES_FILE_NAME, Result->Count);

    return(Result);
}

// NOTE(pvlso): A new entity of a type: its first state, no look, no collision, the type's values
internal entity_file
NewEntityOfType(entity_types *Types, u32 TypeIndex)
{
    entity_file Result = {};
    entity_type *Type = Types->Types + TypeIndex;
    Result.Loaded = true;
    Result.Type = (s32)TypeIndex;
    FormatString(ENTITY_NAME_LENGTH, Result.TypeName, "%s", Type->Name);
    for(u32 FieldIndex = 0; FieldIndex < Type->FieldCount; ++FieldIndex)
    {
        Result.Fields[FieldIndex] = Type->Fields[FieldIndex].Default;
    }

    return(Result);
}

internal entity_file
ReadEntityFile(char *FileName, entity_types *Types, memory_arena *Arena)
{
    entity_file Result = {};
    Result.Type = -1;

    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_Entity, Arena, true);
    if(!File.Contents)
    {
        LOG_ERROR("Entity %s: can't read the file", FileName);
        ++Result.ErrorCount;
        EndTemporaryMemory(TempMem);
        return(Result);
    }

    tileset_line_tokens *Tokens = PushStruct(Arena, tileset_line_tokens);
    char *At = (char *)File.Contents;
    char *FileEnd = At + StringLength(At);
    u32 LineNumber = 0;
    entity_type *Type = 0;
    while(At < FileEnd)
    {
        char *LineEnd = At;
        while((LineEnd < FileEnd) && (*LineEnd != '\n')) {++LineEnd;}
        ++LineNumber;
        TokenizeTilesetLine(At, LineEnd, Tokens);
        At = (LineEnd < FileEnd) ? LineEnd + 1 : LineEnd;
        if(!Tokens->Count)
        {
            continue;
        }

        char *Key = Tokens->Tokens[0];
        b32 Valid = true;
        if(StringsAreEqual(Key, "type") && (Tokens->Count >= 2))
        {
            s32 TypeIndex = FindEntityType(Types, Tokens->Tokens[1]);
            if(TypeIndex >= 0)
            {
                Result = NewEntityOfType(Types, (u32)TypeIndex);
                Type = Types->Types + TypeIndex;
            }
            else
            {
                FormatString(ENTITY_NAME_LENGTH, Result.TypeName, "%s", Tokens->Tokens[1]);
                LOG_ERROR("Entity %s: %s isn't one of data/txts/%s", FileName, Tokens->Tokens[1],
                          ENTITY_TYPES_FILE_NAME);
                ++Result.ErrorCount;
                break;
            }
        }
        else if(!Type)
        {
            LOG_ERROR("Entity %s, line %d: the type comes first", FileName, LineNumber);
            ++Result.ErrorCount;
            break;
        }
        else if(StringsAreEqual(Key, "state") && (Tokens->Count >= 2))
        {
            s32 State = FindEntityState(Type, Tokens->Tokens[1]);
            Valid = (State >= 0);
            if(Valid) {Result.State = (u32)State;}
        }
        else if(StringsAreEqual(Key, "flip") && (Tokens->Count >= 2))
        {
            entity_field_value Value = {};
            Valid = ReadEntityFieldValue(EntityField_Bool, Tokens->Tokens[1], &Value);
            Result.Flip = Value.Bool;
        }
        else if(StringsAreEqual(Key, "collision") && (Tokens->Count >= 2))
        {
            char *Shape = Tokens->Tokens[1];
            u32 Needed = StringsAreEqual(Shape, "circle") ? 5 : (StringsAreEqual(Shape, "box") ? 6 : 2);
            Valid = (Tokens->Count >= Needed);
            for(u32 Index = 2; Valid && (Index < Needed); ++Index)
            {
                Valid = IsTilesetNumber(Tokens->Tokens[Index]);
            }
            if(Valid && StringsAreEqual(Shape, "circle"))
            {
                Result.Collision = EntityCollision_Circle;
                Result.CollisionCenter = V2(R32FromTilesetToken(Tokens->Tokens[2]), R32FromTilesetToken(Tokens->Tokens[3]));
                Result.CollisionSize = V2(R32FromTilesetToken(Tokens->Tokens[4]), 0.0f);
            }
            else if(Valid && StringsAreEqual(Shape, "box"))
            {
                Result.Collision = EntityCollision_Box;
                Result.CollisionCenter = V2(R32FromTilesetToken(Tokens->Tokens[2]), R32FromTilesetToken(Tokens->Tokens[3]));
                Result.CollisionSize = V2(R32FromTilesetToken(Tokens->Tokens[4]), R32FromTilesetToken(Tokens->Tokens[5]));
            }
            else if(Valid && StringsAreEqual(Shape, "none"))
            {
                Result.Collision = EntityCollision_None;
            }
            else
            {
                Valid = false;
            }
        }
        else if(StringsAreEqual(Key, "look") && (Tokens->Count >= 4))
        {
            s32 State = FindEntityState(Type, Tokens->Tokens[1]);
            entity_look_kind Kind = (StringsAreEqual(Tokens->Tokens[2], "bitmap") ? EntityLook_Bitmap :
                                     (StringsAreEqual(Tokens->Tokens[2], "spritesheet") ? EntityLook_SpriteSheet :
                                      EntityLook_None));
            Valid = (State >= 0) && (Kind != EntityLook_None);
            if(Valid)
            {
                entity_look *Look = Result.Looks + State;
                Look->Kind = Kind;
                FormatString(ENTITY_LOOK_NAME_LENGTH, Look->FileName, "%s", Tokens->Tokens[3]);
            }
        }
        else if(StringsAreEqual(Key, "field") && (Tokens->Count >= 3))
        {
            s32 Field = FindEntityField(Type, Tokens->Tokens[1]);
            Valid = (Field >= 0) && ReadEntityFieldValue(Type->Fields[Field].Kind, Tokens->Tokens[2],
                                                         Result.Fields + Field);
        }
        else
        {
            Valid = false;
        }

        if(!Valid)
        {
            LOG_ERROR("Entity %s, line %d: can't read the %s line (not one of %s's?)", FileName, LineNumber, Key,
                      Type->Name);
            ++Result.ErrorCount;
        }
    }
    Result.Loaded = (Type != 0);
    EndTemporaryMemory(TempMem);

    return(Result);
}

// NOTE(pvlso): A text value in quotes, without the quotes it may hold
internal void
CopyWithoutQuotes(char *Source, char *Dest, u32 DestSize)
{
    u32 Length = 0;
    for(char *C = Source; *C && (Length + 1 < DestSize); ++C)
    {
        if(*C != '"') {Dest[Length++] = *C;}
    }
    Dest[Length] = 0;
}

internal b32
WriteEntityFile(char *FileName, entity_file *Entity, entity_types *Types, memory_arena *Arena)
{
    Assert(Entity->Type >= 0);
    entity_type *Type = Types->Types + Entity->Type;

    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    umm Capacity = 4096 + (umm)ENTITY_MAX_STATES*(ENTITY_LOOK_NAME_LENGTH + 64) +
        (umm)ENTITY_MAX_FIELDS*(ENTITY_TEXT_LENGTH + 64);
    char *Text = (char *)PushSize(Arena, Capacity);
    umm At = 0;
#define ENTITY_OUT(...) At += FormatString(Capacity - At, Text + At, __VA_ARGS__)
    ENTITY_OUT("; An entity (Kaban engine), written by the entity editor. Format: engine_entity_file.h\n");
    ENTITY_OUT("type %s\n", Type->Name);
    ENTITY_OUT("state %s\n", Type->States[Entity->State]);
    ENTITY_OUT("flip %s\n", Entity->Flip ? "yes" : "no");
    switch(Entity->Collision)
    {
        case EntityCollision_None:
        {
            ENTITY_OUT("collision none\n");
        } break;

        case EntityCollision_Circle:
        {
            ENTITY_OUT("collision circle %.2f %.2f %.2f\n", Entity->CollisionCenter.x, Entity->CollisionCenter.y,
                       Entity->CollisionSize.x);
        } break;

        case EntityCollision_Box:
        {
            ENTITY_OUT("collision box %.2f %.2f %.2f %.2f\n", Entity->CollisionCenter.x, Entity->CollisionCenter.y,
                       Entity->CollisionSize.x, Entity->CollisionSize.y);
        } break;

        InvalidDefaultCase;
    }
    for(u32 StateIndex = 0; StateIndex < Type->StateCount; ++StateIndex)
    {
        entity_look *Look = Entity->Looks + StateIndex;
        if(Look->Kind != EntityLook_None)
        {
            char Name[ENTITY_LOOK_NAME_LENGTH];
            CopyWithoutQuotes(Look->FileName, Name, sizeof(Name));
            ENTITY_OUT("look %s %s \"%s\"\n", Type->States[StateIndex],
                       (Look->Kind == EntityLook_Bitmap) ? "bitmap" : "spritesheet", Name);
        }
    }
    for(u32 FieldIndex = 0; FieldIndex < Type->FieldCount; ++FieldIndex)
    {
        entity_type_field *Field = Type->Fields + FieldIndex;
        entity_field_value *Value = Entity->Fields + FieldIndex;
        switch(Field->Kind)
        {
            case EntityField_Int:    {ENTITY_OUT("field %s %d\n", Field->Name, Value->Int);}                    break;
            case EntityField_Number: {ENTITY_OUT("field %s %.3f\n", Field->Name, Value->Number);}               break;
            case EntityField_Bool:   {ENTITY_OUT("field %s %s\n", Field->Name, Value->Bool ? "yes" : "no");}    break;
            case EntityField_Text:
            {
                char Clean[ENTITY_TEXT_LENGTH];
                CopyWithoutQuotes(Value->Text, Clean, sizeof(Clean));
                ENTITY_OUT("field %s \"%s\"\n", Field->Name, Clean);
            } break;
            InvalidDefaultCase;
        }
    }
#undef ENTITY_OUT

    b32 Result = (Platform.WriteEntireFile(FileName, PlatformFileType_Entity, (u8 *)Text, (u32)At) != 0);
    if(!Result)
    {
        LOG_ERROR("Entity %s: can't write the file", FileName);
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}
