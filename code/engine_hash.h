#if !defined(ENGINE_HASH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum hash_key_type
{
    HashKeyType_S32,
    HashKeyType_STRING,
    HashKeyType_WORLD_EDGE,
};

enum hash_data_type
{
    HashDataType_S32,
    HashDataType_U32,
    HashDataType_STRING,
    HashDataType_POLY_MESH_ADJACENCY,
};

struct hash_key_world_edge
{
    world_position A;
    world_position B;
};

struct hash_data_poly_mesh_adjacency
{
    u32 PolyAID;
    world_position ALeft;
    world_position ARight;

    u32 PolyBID;
    world_position BLeft;
    world_position BRight;
};

union hash_key
{
    char *String;
    hash_key_world_edge WorldEdge;
};

union hash_data
{
    u32 Check;
    hash_data_poly_mesh_adjacency PolyMeshAdjacency;
};

struct hash_table_entry
{
    hash_key Key;
    hash_data Data;
    hash_table_entry *Next;
};

struct hash_table
{
    u32 Size;
    hash_key_type KeyType;
    hash_data_type DataType;
    hash_table_entry **Hash;

    hash_table_entry *Free;
};


inline u32
CalculateHashForString(void *Key)
{
    u32 Result = 0;

    char *Str = (char *)Key;
    u32 Hash = 5381;
    s32 C = *Str; 
    while(*Str)
    {
        Hash = ((Hash << 5) + Hash) + C;
        *Str++;
        C = *Str;
    }

    Result = Hash;

    return(Result);
}

inline u32
CombineHashes(u32 A, u32 B)
{
    u32 Result = A ^ (B + 0x9E3779b9 + (A << 6) + (B >> 2));
    return(Result);
}

inline b32
HashCompareStrings(void *Key1, void *Key2)
{
    b32 Result = StringsAreEqual((char *)Key1, (char *)Key2);

    return(Result);
}

inline u32
HashWorldPosition(world_position *P)
{
    s32 Scale = 10000;
    s32 x_s32 = (s32)RoundReal32ToInt32(P->Offset.x * Scale);
    s32 y_s32 = (s32)RoundReal32ToInt32(P->Offset.y * Scale);

    u32 Hash = 17;
    Hash = CombineHashes(Hash, P->TileX);
    Hash = CombineHashes(Hash, P->TileY);
    Hash = CombineHashes(Hash, x_s32);
    Hash = CombineHashes(Hash, y_s32);

    return(Hash);
}

inline u32
HashWorldEdge(hash_key_world_edge *E)
{
    u32 HashA = HashWorldPosition(&E->A);
    u32 HashB = HashWorldPosition(&E->B);

    u32 MinHash = HashA < HashB ? HashA : HashB;
    u32 MaxHash = HashA < HashB ? HashB : HashA;

    u32 Result = 17;
    Result = CombineHashes(Result, MinHash);
    Result = CombineHashes(Result, MaxHash);

    return(Result);
}

inline b32
HashWorldPosAreEqual(world_position *A, world_position *B)
{
    b32 Result = ((A->TileX == B->TileX) && (A->TileY == B->TileY) &&
                  (AbsoluteValue(A->Offset.x - B->Offset.x) < 0.01f) &&
                  (AbsoluteValue(A->Offset.y - B->Offset.y) < 0.01f));
    return(Result);
}

inline b32
HashWorldEdgesAreEqual(hash_key_world_edge *A, hash_key_world_edge *B)
{
    b32 Result = ((HashWorldPosAreEqual(&A->A, &B->A) &&
                   HashWorldPosAreEqual(&A->B, &B->B)) ||
                  (HashWorldPosAreEqual(&A->A, &B->B) &&
                   HashWorldPosAreEqual(&A->B, &B->A)));
    return(Result);
}

inline u32
CalculateHash(hash_key Key, hash_key_type KeyType)
{
    u32 Result = 0;
    switch(KeyType)
    {
        case HashKeyType_S32:
        {
        } break;

        case HashKeyType_STRING:
        {
        } break;

        case HashKeyType_WORLD_EDGE:
            Result = HashWorldEdge(&Key.WorldEdge);
            break;

        InvalidDefaultCase;
    }

    return(Result);
}

inline b32
CompareKeys(hash_key Key0, hash_key Key1, hash_key_type KeyType0, hash_key_type KeyType1)
{
    b32 Result = false;

    if(KeyType0 == KeyType1)
    {
        switch(KeyType0)
        {
            case HashKeyType_S32:
            {
            } break;

            case HashKeyType_STRING:
            {
            } break;

            case HashKeyType_WORLD_EDGE:
                Result = HashWorldEdgesAreEqual(&Key0.WorldEdge, &Key1.WorldEdge);
                break;
                
            InvalidDefaultCase;
        }
    }

    return(Result);
}

inline void
InsertKey(hash_table *Table, hash_key Key, hash_data Data, memory_arena *Arena)
{
    TIMED_FUNCTION();
    
    u32 HashIndex = CalculateHash(Key, Table->KeyType);
    HashIndex %= Table->Size;
    
    hash_table_entry *NewEntry = Table->Free;
    if(NewEntry)
        Table->Free = NewEntry->Next;
    else
        NewEntry = PushStruct(Arena, hash_table_entry); 

    NewEntry->Key = Key;
    NewEntry->Data = Data;
    NewEntry->Next = Table->Hash[HashIndex];
    Table->Hash[HashIndex] = NewEntry;
}

inline hash_data
GetHashElement(hash_table *Table, hash_key Key)
{
    TIMED_FUNCTION();

    hash_data Result = {};

    u32 HashIndex = CalculateHash(Key, Table->KeyType);
    HashIndex %= Table->Size;
    hash_table_entry *Entry = Table->Hash[HashIndex];
    u32 Count = 0;
    while(Entry)
    {
        if(CompareKeys(Entry->Key, Key, Table->KeyType, Table->KeyType))
        {
            Result = Entry->Data;
            break;
        }

        Entry = Entry->Next;
        ++Count;
    }

    Assert(Count < 8);
    
    return(Result);
}

#define ENGINE_HASH_H
#endif
