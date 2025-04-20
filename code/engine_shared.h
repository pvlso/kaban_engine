#if !defined(ENGINE_SHARED_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_intrinsics.h"
#include "engine_math.h"
#include "engine_triangle.h"

#include "engine_random.h"
#include "engine_memory.h"
#include "engine_string.h"
#include "engine_crc.h"

inline u32
SortKeyToU32(r32 SortKey)
{
    // NOTE(casey): We need to turn our 32-bit floating point value
    // into some strictly ascending 32-bit unsigned integer value
    u32 Result = *(u32 *)&SortKey;
    if(Result & 0x80000000)
    {
        Result = ~Result;
    }
    else
    {
        Result |= 0x80000000;
    }

    return(Result);
}

global_variable v3 DebugColorTable[] =
{
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 0},
    {0, 1, 1},
    {1, 0, 1},
    {1, 0.5f, 0},
    {1, 0, 0.5f},
    {0.5f, 1, 0},
    {0, 1, 0.5f},
    {0.5f, 0, 1},
    {0.1028, 0.5312, 0.8078},
    {0.3125, 0.8844, 0.4784},
    {0.1868, 0.2624, 0.8449},
    {0.0626, 0.0854, 0.8665},
    {0.6298, 0.8287, 0.8049},
    {0.5582, 0.7462, 0.5137},
    {0.7372, 0.5665, 0.9878},
    {0.0481, 0.5489, 0.9372},
    {0.1656, 0.0822, 0.0605},
    {0.0562, 0.0390, 0.9506},
    {0.4321, 0.2841, 0.9994},
    {0.0752, 0.9016, 0.8405},
    {0.5898, 0.9335, 0.0619},
    {0.3688, 0.4160, 0.9041},
    {0.4484, 0.9473, 0.1858},
    {0.5330, 0.6409, 0.0581},
    {0.1431, 0.1299, 0.2374},
    {0.2019, 0.6299, 0.4140},
    {0.4713, 0.6883, 0.4025},
    {0.4912, 0.1313, 0.4535},
    {0.2124, 0.3513, 0.8837},
    {0.3189, 0.7460, 0.2089},
    {0.0168, 0.8861, 0.5419},
    {0.7720, 0.4520, 0.7699},
    {0.5791, 0.2399, 0.0992},
    {0.6426, 0.5293, 0.5107},
    {0.9323, 0.6897, 0.8025},
    {0.1079, 0.6979, 0.2374},
    {0.2686, 0.7170, 0.6474},
    {0.8829, 0.0909, 0.8297},
    {0.1476, 0.9972, 0.1183},
    {0.9105, 0.5648, 0.3042},
    {0.0007, 0.3076, 0.3010},
    {0.9104, 0.3937, 0.1161},
    {0.5510, 0.3267, 0.4397},
    {0.7701, 0.3489, 0.4813},
    {0.5665, 0.7097, 0.1812},
    {0.7207, 0.0840, 0.4588},
    {0.2051, 0.5209, 0.5612},
    {0.0493, 0.4915, 0.9108},
    {0.1411, 0.7480, 0.0155},
    {0.2527, 0.3339, 0.3942},
    {0.0710, 0.1963, 0.2849},
    {0.0127, 0.6260, 0.6980},
    {0.4880, 0.8594, 0.3048},
    {0.2869, 0.9362, 0.3384},
    {0.1113, 0.5194, 0.4928},
    {0.3103, 0.1108, 0.6696},
    {0.3163, 0.1742, 0.5541},
    {0.5243, 0.4920, 0.2905},
    {0.3973, 0.9598, 0.1572},
    {0.4384, 0.1351, 0.7984},
    {0.2959, 0.9694, 0.9492},
    {0.7462, 0.4598, 0.7370},
    {0.3733, 0.8101, 0.9447},
    {0.9680, 0.3013, 0.9917},
    {0.4595, 0.7455, 0.6151},
    {0.5945, 0.1787, 0.4894},
    {0.1900, 0.7216, 0.1632},
    {0.6748, 0.4077, 0.3198},
    {0.8792, 0.1834, 0.6540},
    {0.5933, 0.4781, 0.3027},
    {0.6502, 0.8242, 0.3608},
    {0.3904, 0.8362, 0.3996},
};

struct sort_entry
{
    r32 SortKey;
    u32 Index;

    union
    {
        v3 P;
    };
};

inline void
S32RemoveAt(s32 *Array, s32 Count, s32 Index)
{
    Array[Index] = 0;
    for(s32 I = Index;
        I < Count - 1;
        ++I)
    {
        Array[I] = Array[I + 1];
    }

    Array[Count] = 0;
}

inline void
Swap(sort_entry *A, sort_entry *B)
{
    sort_entry Temp = *B;
    *B = *A;
    *A = Temp;
}

inline r32
CalculateBitmapScaleForSquareCanvas(r32 CanvasSize, u32 Width, u32 Height)
{
    r32 Result = 1.0f;
    if(Width > Height)
    {
        Result = CanvasSize / Width;
    }
    else
    {
        Result = CanvasSize / Height;
    }

    return(Result);
}

struct edge
{
    v2 a;
    v2 b;
};

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

#define ENGINE_SHARED_H
#endif
