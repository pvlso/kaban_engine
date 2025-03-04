#if !defined(EDITOR_STRING_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
#include <stdarg.h>

inline b32
IsEndOfLine(char C)
{
    b32 Result = ((C == '\n') ||
                  (C == '\r'));

    return(Result);
}

inline b32
IsWhitespace(char C)
{
    b32 Result = ((C == ' ') ||
                  (C == '\t') ||
                  (C == '\v') ||
                  (C == '\f') ||
                  IsEndOfLine(C));

    return(Result);
}

inline b32
StringsAreEqual(char *A, char *B)
{
    b32 Result = (A == B);

    if(A && B)
    {
        while(*A && *B && (*A == *B))
        {
            ++A;
            ++B;
        }

        Result = ((*A == 0) && (*B == 0));
    }
    
    return(Result);
}

inline b32
StringsAreEqual(umm ALength, char *A, char *B)
{
    b32 Result = false;
    
    if(B)
    {
        char *At = B;
        for(umm Index = 0;
            Index < ALength;
            ++Index, ++At)
        {
            if((*At == 0) ||
               (A[Index] != *At))
            {
               return(false);
            }        
        }
            
        Result = (*At == 0);
    }
    else
    {
        Result = (ALength == 0);
    }
    
    return(Result);
}

inline b32
StringsAreEqual(umm ALength, char *A, umm BLength, char *B)
{
    b32 Result = (ALength == BLength);

    if(Result)
    {
        Result = true;
        for(u32 Index = 0;
            Index < ALength;
            ++Index)
        {
            if(A[Index] != B[Index])
            {
                Result = false;
                break;
            }
        }
    }

    return(Result);
}

internal s32
S32FromZInternal(char **AtInit)
{
    s32 Result = 0;
    char *At = *AtInit;
    while((*At >= '0') && (*At <= '9'))
    {
        Result *= 10;
        Result += (*At - '0');
        ++At;
    }

    *AtInit = At;
    
    return(Result);
}

internal s32
S32FromZ(char *At)
{
    char *Ignored = At;
    s32 Result = S32FromZInternal(&At);

    return(Result);
}

struct format_dest
{
    umm Size;
    char *At;
};

inline void
OutChar(format_dest *Dest, char Value)
{
    if(Dest->Size)
    {
        --Dest->Size;
        *Dest->At++ = Value;
    }
}

inline void
OutChars(format_dest *Dest, char *Value)
{
    while(*Value)
    {
        OutChar(Dest, *Value++);
    }
}
         
inline u64
ReadVarArgUnsignedInteger(u32 Length, va_list *ArgList)
{
    u64 Result = 0;
    switch(Length)
    {
        case 1:
        case 2:
        case 4:
        {
            Result = va_arg(*ArgList, u32);
        } break;

        case 8:
        {
            Result = va_arg(*ArgList, u64);
        } break;
    }

    return(Result);
}

inline s64
ReadVarArgSignedInteger(u32 Length, va_list *ArgList)
{
    u64 Temp = ReadVarArgUnsignedInteger(Length, ArgList);
    s64 Result = *(s64 *)&Temp;
    
    return(Result);
}

inline f64
ReadVarArgFloat(u32 Length, va_list *ArgList)
{
    r64 Result = 0;
    switch(Length)
    {
        case 4:
        {
            Result = va_arg(*ArgList, f32);
        } break;

        case 8:
        {
            Result = va_arg(*ArgList, f64);
        } break;
    }

    return(Result);
}
                
char DecChars[] = "0123456789";
char LowerHexChars[] = "0123456789abcdef";
char UpperHexChars[] = "0123456789ABCDEF";

inline void
U64ToASCII(format_dest *Dest, u64 Value, u32 Base, char *Digits)
{
//    TIMED_FUNCTION();
    Assert(Base != 0);

    char *Start = Dest->At;
    do
    {
        u64 DigitIndex = (Value % Base);
        char Digit = Digits[DigitIndex];
        OutChar(Dest, Digit);

        Value /= Base;
    } while(Value != 0);

    char *End = Dest->At;
    while(Start < End)
    {
        --End;
        char Temp = *End;
        *End = *Start;
        *Start = Temp;
        ++Start;
    }
}

inline void
F64ToASCII(format_dest *Dest, f64 Value, u32 Precision)
{
//    TIMED_FUNCTION();
    if(Value < 0)
    {
        OutChar(Dest, '-');
        Value = -Value;
    }

    u64 IntegerPart = (u64)Value;
    Value -= (f64)IntegerPart;
    U64ToASCII(Dest, IntegerPart, 10, DecChars);

    OutChar(Dest, '.');

    for(u32 PrecisionIndex = 0;
        PrecisionIndex < Precision;
        ++PrecisionIndex)
    {
        Value *= 10.0f;
        u32 Integer = (u32)Value;
        Value -= (f32)Integer;
        OutChar(Dest, DecChars[Integer]);
    }
}

internal umm
FormatStringList(umm DestSize, char *DestInit, char *Format, va_list ArgList)
{
    format_dest Dest = {DestSize, DestInit};

    if(Dest.Size)
    {
        char *At = Format;
        while(At[0])
        {
            if(At[0] == '%')
            {
                ++At;

                b32 ForceSign = false;
                b32 PadWithZeros = false;
                b32 LeftJustify = false;
                b32 PositiveSignIsBlank = false;
                b32 AnotateIfNotZero = false;

                b32 Parsing = true;

                //
                // NOTE(casey): Handle the flags
                //
                Parsing = true;
                while(Parsing)
                {
                    switch(*At)
                    {
                        case '+': {ForceSign = true;} break;
                        case '0': {PadWithZeros = true;} break;
                        case '-': {LeftJustify = true;} break;
                        case ' ': {PositiveSignIsBlank = true;} break;
                        case '#': {AnotateIfNotZero = true;} break;
                        default: {Parsing = false;} break;
                    }

                    if(Parsing)
                    {
                        ++At;
                    }
                }

                //
                // NOTE(casey): Handle the width
                //
                b32 WidthSpecified = false;
                s32 Width = 0;
                if(*At == '*')
                {
                    Width = va_arg(ArgList, int);
                    WidthSpecified = true;
                    ++At;
                }
                else if((*At >= '0') && (*At <= '9'))
                {
                    Width = S32FromZInternal(&At);
                    WidthSpecified = true;
                }

                //
                // NOTE(casey): Handle the precision
                //
                b32 PrecisionSpecified = false;
                s32 Precision = 0;
                if(*At == '.')
                {
                    ++At;
                    if(*At == '*')
                    {
                        Precision = va_arg(ArgList, int);
                        PrecisionSpecified = true;
                        ++At;
                    }
                    else if((*At >= '0') && (*At <= '9'))
                    {
                        Precision = S32FromZInternal(&At);
                        PrecisionSpecified = true;
                    }
                    else
                    {
                        Assert(!"Malformed precision specifier!");
                    }
                }

                if(!PrecisionSpecified)
                {
                    Precision = 6;
                }
                
                //
                // NOTE(casey): Handle the length
                //

                u32 IntegerLength = 4;
                u32 FloatLength = 8;
                if((At[0] == 'h') && (At[1] == 'h'))
                {
                    At += 2;
                }
                else if((At[0] == 'l') && (At[1] == 'l'))
                {
                    At += 2;
                }
                else if(*At == 'h')
                {
                    ++At;
                }
                else if(*At == 'l')
                {
                    ++At;
                }
                else if(*At == 'j')
                {
                    ++At;
                }
                else if(*At == 'z')
                {
                    ++At;
                }
                else if(*At == 't')
                {
                    ++At;
                }
                else if(*At == 'L')
                {
                    ++At;
                }

                char TempBuffer[64];
                char *Temp = TempBuffer;
                format_dest TempDest= {ArrayCount(TempBuffer), Temp};

                char *TempPrefix = "";
                b32 IsFloat = false;
                switch(*At)
                {
                    case 'd':
                    case 'i':
                    {
                        s64 Value = ReadVarArgSignedInteger(IntegerLength, &ArgList);
                        b32 WasNegative = (Value < 0);
                        if(WasNegative)
                        {
                            Value = -Value;
                        }

                        U64ToASCII(&TempDest, (u64)Value, 10, DecChars);

                        if(WasNegative)
                        {
                            TempPrefix = "-";
                        }
                        else if(ForceSign)
                        {
                            Assert(!PositiveSignIsBlank);
                            TempPrefix = "+";
                        }
                        else if(PositiveSignIsBlank)
                        {
                            TempPrefix = " ";
                        }
                        
                    } break;

                    case 'u':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &ArgList);
                        U64ToASCII(&TempDest, (u64)Value, 10, DecChars);
                    } break;

                    case 'o':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &ArgList);
                        U64ToASCII(&TempDest, (u64)Value, 8, DecChars);
                        if(AnotateIfNotZero && (Value != 0))
                        {
                            TempPrefix = "0";
                        }
                    } break;

                    case 'x':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &ArgList);
                        U64ToASCII(&TempDest, (u64)Value, 16, LowerHexChars);
                        if(AnotateIfNotZero && (Value != 0))
                        {
                            TempPrefix = "0x";
                        }
                    } break;

                    case 'X':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &ArgList);
                        U64ToASCII(&TempDest, (u64)Value, 16, UpperHexChars);
                        if(AnotateIfNotZero && (Value != 0))
                        {
                            TempPrefix = "0X";
                        }
                    } break;

                    case 'f':
                    case 'F':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                    {
                        f64 Value = ReadVarArgFloat(FloatLength, &ArgList);
                        F64ToASCII(&TempDest, Value, Precision);
                        IsFloat = true;
                    } break;

                    case 'c':
                    {
                        int Value = va_arg(ArgList, int);
                        OutChar(&TempDest, (char)Value);
                    } break;

                    case 's':
                    {
                        char *String = va_arg(ArgList, char *);

                        Temp = String;
                        if(PrecisionSpecified)
                        {
                            TempDest.Size = 0;
                            for(char *Scan = String;
                                *Scan && (TempDest.Size < Precision);
                                ++Scan)
                            {
                                ++TempDest.Size;
                            }
                        }
                        else
                        {
                            TempDest.Size = StringLength(String);
                        }

                        TempDest.At = String + TempDest.Size;
                    } break;

                    case 'p':
                    {
                        void *Value = va_arg(ArgList, void *);
                        U64ToASCII(&TempDest, *(umm *)&Value, 16, LowerHexChars);
                    } break;

                    case 'n':
                    {
                        int *TabDest = va_arg(ArgList, int *);
                        *TabDest = (int)(Dest.At - DestInit);
                    } break;

                    case '%':
                    {
                        OutChar(&Dest, '%');
                    } break;

                    default:
                    {
                        Assert(!"Unrecognized format specifier");
                    } break;
                }

                if(TempDest.At - Temp)
                {
                    smm UsePrecision = Precision;
                    if(IsFloat || !PrecisionSpecified)
                    {
                        UsePrecision = (TempDest.At - Temp);
                    }

                    smm PrefixLength = StringLength(TempPrefix);
                    smm UseWidth = Width;
                    smm ComputedWidth = UsePrecision + PrefixLength;
                    if(UseWidth < ComputedWidth)
                    {
                        UseWidth = ComputedWidth;
                    }

                    if(PadWithZeros)
                    {
                        Assert(!LeftJustify);
                        LeftJustify = false;
                    }
                
                    if(!LeftJustify)
                    {
                        while(UseWidth > (UsePrecision + PrefixLength))
                        {
                            OutChar(&Dest, PadWithZeros ? '0' : ' ');
                            --UseWidth;
                        }
                    }
                
                    for(char *Pre = TempPrefix;
                        *Pre && UseWidth;
                        ++Pre)
                    {
                        OutChar(&Dest, *Pre);
                        --UseWidth;
                    }

                    if(UsePrecision > UseWidth)
                    {
                        UsePrecision = UseWidth;
                    }

                    while(UsePrecision > (TempDest.At - Temp))
                    {
                        OutChar(&Dest, '0');
                        --UsePrecision;
                        --UseWidth;
                    }

                    while(UsePrecision && (TempDest.At != Temp))
                    {
                        OutChar(&Dest, *Temp++);
                        --UsePrecision;
                        --UseWidth;
                    }

                    if(LeftJustify)
                    {
                        while(UseWidth)
                        {
                            OutChar(&Dest, ' ');
                            --UseWidth;
                        }
                    }
                }
                
                if(*At)
                {
                    ++At;
                }
            }
            else
            {
                OutChar(&Dest, *At++);
            }
        }

        if(Dest.Size)
        {
            Dest.At[0] = 0;
        }
        else
        {
            Dest.At[-1] = 0;
        }
    }

    umm Result = Dest.At - DestInit;
    return(Result);
}

internal umm
FormatString(umm DestSize, char *Dest, char *Format, ...)
{
    va_list ArgList;

    va_start(ArgList, Format);
    umm Result = FormatStringList(DestSize, Dest, Format, ArgList);
    va_end(ArgList);

    return(Result);
}

#define EDITOR_STRING_H
#endif
