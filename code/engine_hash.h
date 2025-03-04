#if !defined(EDITOR_HASH_H)
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
    HashKeyType_EDGEV2,
    HashKeyType_EDGEFP22_10,
};

enum hash_data_type
{
    HashDataType_S32,
    HashDataType_U32,
    HashDataType_STRING,
};

union hash_key
{
    char *String;
    edgefp22_10 Edgefp22_10;
    edge EdgeV2;
};

union hash_data
{
    u32 Check;
    fp22_10 FP22_10;
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
    hash_key_type DataType;
    hash_table_entry **Hash;
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
CalculateHashForEDGEFP22_10_V2(edgefp22_10 Edge)
{
    u32 Result = 0;

    u32 A = (u32)Edge.a.x;
    u32 B = (u32)Edge.a.y;
    u32 C = (u32)Edge.b.x;
    u32 D = (u32)Edge.b.y;

    u32 First = A ^ ((B << 16) | (B >> 16));
    u32 Second = C ^ ((D << 16) | (D >> 16));

    Result = First ^ ((Second << 16) | (Second >> 16));
    Result ^= (Result >> 17);
    Result *= 0xED5AD4BB;
    Result ^= (Result >> 11);
    Result *= 0xAC4C1B51;
    Result ^= (Result >> 15);
    
    return(Result);
}

inline b32
HashCompareEdgeFp22_10(edgefp22_10 A, edgefp22_10 B)
{
    b32 Result = false;

    Result = ((A.a.x == B.a.x) && (A.a.y == B.a.y) &&
              (A.b.x == A.b.x) && (B.b.y == B.b.y));

    return(Result);
}

inline u32
CombineHashes(u32 A, u32 B)
{
    u32 Result = A ^ (B + 0x9E3779b9 + (A << 6) + (B >> 2));
    return(Result);
}

inline u32
EdgeHashFucntion(edge Edge)
{
    TIMED_FUNCTION();

    u32 Result = 0;
    u32 H0 = (u32)(roundf(Edge.a.x*1000.0f));//SortKeyToU32(Edge.a.x);
    u32 H1 = (u32)(roundf(Edge.a.y*1000.0f));//SortKeyToU32(Edge.a.x);
    u32 H2 = (u32)(roundf(Edge.b.x*1000.0f));//SortKeyToU32(Edge.a.x);
    u32 H3 = (u32)(roundf(Edge.b.y*1000.0f));//SortKeyToU32(Edge.a.x);

    u32 A = CombineHashes(H0, H1);
    u32 B = CombineHashes(H2, H3);
    Result = CombineHashes(A, B);
    
    return(Result);
}

inline b32
HashCompareStrings(void *Key1, void *Key2)
{
    b32 Result = StringsAreEqual((char *)Key1, (char *)Key2);

    return(Result);
}

inline b32
HashCompareEdges(edge Edge0, edge Edge1)
{
    b32 Result = false;

    Result = (TRISUBPointsAreEqual(Edge0.a, Edge1.a) && TRISUBPointsAreEqual(Edge0.b, Edge1.b));
    return(Result);
}

inline u32
CalculateHash(hash_key Key, hash_key_type KeyType)
{
    u32 Result = 0;
    switch(KeyType)
    {
        case HashKeyType_EDGEV2:
        {
            Result = EdgeHashFucntion(Key.EdgeV2);
        } break;

        case HashKeyType_EDGEFP22_10:
        {
            Result = CalculateHashForEDGEFP22_10_V2(Key.Edgefp22_10);
        } break;
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
            case HashKeyType_EDGEV2:
            {
                Result = HashCompareEdges(Key0.EdgeV2, Key1.EdgeV2);
            } break;

            case HashKeyType_EDGEFP22_10:
            {
                Result = HashCompareEdgeFp22_10(Key0.Edgefp22_10, Key1.Edgefp22_10);
            } break;
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

//    Assert(Key);
//    Assert(Value);
    
    hash_table_entry *NewEntry = PushStruct(Arena, hash_table_entry); 
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

#define EDITOR_HASH_H
#endif
