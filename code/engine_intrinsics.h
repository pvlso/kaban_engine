#if !defined(EDITOR_INTRINSICS_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

//
// TODO(casey): Convert all of these to platform-efficient versions
// and remove math.h
//

#include "math.h"

inline int32
SignOf(int32 Value)
{
    int32 Result = (Value >= 0) ? 1 : -1;
    return(Result);
}

inline real32
SignOf(real32 Value)
{
    real32 Result = (Value >= 0) ? 1.0f : -1.0f;
    return(Result);
}

inline f32
SquareRoot(real32 Real32)
{
    f32 Result = sqrtf(Real32);
    return(Result);
}

inline f64
SquareRoot(f64 Float64)
{
    f64 Result = sqrt(Float64);
    return(Result);
}

inline real32
AbsoluteValue(real32 Real32)
{
    real32 Result = (r32)fabs(Real32);
    return(Result);
}

inline f64
AbsoluteValue(f64 Float64)
{
    f64 Result = fabs(Float64);
    return(Result);
}

inline int32
AbsoluteValue(int32 Int32)
{
    int32 Result = abs(Int32);
    return(Result);
}

inline uint32
RotateLeft(uint32 Value, int32 Amount)
{
#if COMPILER_MSVC
    uint32 Result = _rotl(Value, Amount);
#else
    // TODO(casey): Actually port this to other compiler platforms!
    Amount &= 31;
    uint32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
#endif

    return(Result);
}

inline uint32
RotateRight(uint32 Value, int32 Amount)
{
#if COMPILER_MSVC
    uint32 Result = _rotr(Value, Amount);
#else
    // TODO(casey): Actually port this to other compiler platforms!
    Amount &= 31;
    uint32 Result = ((Value >> Amount) | (Value << (32 - Amount)));
#endif

    return(Result);
}

inline int32
RoundReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)roundf(Real32);
    return(Result);
}

inline uint32
RoundReal32ToUInt32(real32 Real32)
{
    uint32 Result = (uint32)roundf(Real32);
    return(Result);
}

inline int32 
FloorReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)floorf(Real32);
    return(Result);
}

inline int32 
CeilReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)ceilf(Real32);
    return(Result);
}

inline int32
TruncateReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)Real32;
    return(Result);
}

inline real32
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return(Result);
}

inline real32
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return(Result);
}

inline real32
ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return(Result);
}

inline f64
Sin(f64 Angle)
{
    f64 Result = sin(Angle);
    return(Result);
}

inline f64
Cos(f64 Angle)
{
    f64 Result = cos(Angle);
    return(Result);
}

inline f64
ATan2(f64 Y, f64 X)
{
    f64 Result = atan2(Y, X);
    return(Result);
}

inline f64
AToF(char *A)
{
    f64 Result = atof(A);

    return(Result);
}

struct bit_scan_result
{
    bool32 Found;
    uint32 Index;
};

inline bit_scan_result
FindLeastSignificantSetBit(uint32 Value)
{
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
    for(uint32 Test = 0;
        Test < 32;
        ++Test)
    {
        if(Value & (1 << Test))
        {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif
    
    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): MULTITHREADING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= U32Maximum);
    uint32 Result = (uint32)Value;
    return(Result);
}

inline u16
SafeTruncateToU16(uint32 Value)
{
    Assert(Value <= U16Maximum);
    u16 Result = (u16)Value;
    return(Result);
}

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32
AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);

    return(Result);
}

inline u64
AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = _InterlockedExchange64((__int64 volatile *)Value, New);

    return(Result);
}

inline u64
AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);

    return(Result);
}    

inline u32
AtomicIncrementU32(u32 volatile *Value)
{
    // NOTE(pvlso): Returns the value _after_ the increment
    u32 Result = _InterlockedIncrement((long volatile *)Value);

    return(Result);
}

inline u32
GetThreadID(void)
{
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 ThreadID = *(u32 *)(ThreadLocalStorage + 0x48);

    return(ThreadID);
}

#elif COMPILER_LLVM
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile("" ::: "memory")

inline uint32
AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);

    return(Result);
}

inline u64
AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = __sync_lock_test_and_set(Value, New);

    return(Result);
}

inline u64
AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = __sync_fetch_and_add(Value, Addend);

    return(Result);
}    

inline u32
AtomicIncrementU32(u32 volatile *Value)
{
    // NOTE(pvlso): Returns the value _after_ the increment
    u32 Result = __sync_add_and_fetch(Value, 1);

    return(Result);
}

inline u32
GetThreadID(void)
{
    u32 ThreadID;
#if defined(__APPLE__) && defined(__x86_64__)
    asm("mov %%gs:0x00,%0" : "=r"(ThreadID));
#elif defined(__i386__)
    asm("mov %%gs:0x08,%0" : "=r"(ThreadID));
#elif defined(__x86_64__)
    asm("mov %%fs:0x10,%0" : "=r"(ThreadID));
#else
#error Unsupported architecture
#endif

    return(ThreadID);
}
#else
// TODO(casey): Other compilers/platforms??
#endif

struct ticket_mutex
{
    u64 volatile Ticket;
    u64 volatile Serving;
};

inline void
BeginTicketMutex(ticket_mutex *Mutex)
{
    u64 Ticket = AtomicAddU64(&Mutex->Ticket, 1);
    while(Ticket != Mutex->Serving) {_mm_pause();};
}

inline void
EndTicketMutex(ticket_mutex *Mutex)
{
    AtomicAddU64(&Mutex->Serving, 1);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define EDITOR_INTRINSICS_H
#endif
