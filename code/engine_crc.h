#if !defined(EDITOR_CRC_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define CRC32_POLY 0xEDB88320
#define CRC64_POLY 0x42F0E1EBA9EA3693

global_variable u64 CRC64Table[256];

internal void
GenerateCRC64Table(void)
{
    for(u32 I = 0;
        I < 256;
        ++I)
    {
        u64 CRC = I;
        for(u32 J = 0;
            J < 8;
            ++J)
        {
            if(CRC & 1)
            {
                CRC = (CRC >> 1) ^ CRC64_POLY;
            }
            else
            {
                CRC >>= 1;
            }
        }

        CRC64Table[I] = CRC;
    }
}

inline u32
CalculateCRC32(u32 CRC)
{
    u32 Result = 0;

    for(u32 I = 0;
        I < 8;
        ++I)
    {
        u32 Mask = -((s32)(CRC & 1));
        CRC = (CRC >> 1) ^ (CRC32_POLY & Mask);
    }

    Result = CRC;
    
    return(Result);
}

inline u32
CRC32(u8 *Data, umm Size)
{
    u32 CRC = U32Maximum;
    while(Size != 0)
    {
        u8 Byte = *Data;

        CRC = CRC ^ Byte;
        CRC = CalculateCRC32(CRC);

        ++Data;
        --Size;
    }

    CRC = ~CRC;
    
    return(CRC);
}

inline u32
CRC32FromString(char *String)
{
    u32 CRC = U32Maximum;
    while(*String)
    {
        u8 Byte = *String;

        CRC = CRC ^ Byte;
        CRC = CalculateCRC32(CRC);

        ++String;
    }

    CRC = ~CRC;
    
    return(CRC);
}

inline u64
CalculateCRC64(u64 CRC, u8 Byte)
{
    u64 Result = CRC64Table[(CRC ^ Byte) & 0xFF] ^ (CRC >> 8);

    return(Result);
}

inline u64
CRC64(u8 *Data, umm Size)
{
    u64 CRC = U64Maximum;
    while(Size != 0)
    {
        CRC = CalculateCRC64(CRC, *Data);
        ++Data;
        --Size;
    }

    CRC ^= U64Maximum;
    
    return(CRC);
}

inline u64
CRC64FromString(char *String)
{
    u64 CRC = U64Maximum;
    while(*String)
    {
        CRC = CalculateCRC64(CRC, (u8)*String);
        ++String;
    }

    CRC ^= U64Maximum;
    
    return(CRC);
}

#define EDITOR_CRC_H
#endif
