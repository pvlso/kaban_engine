#if !defined(EDITOR_RANDOM_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "pcg_variants.h"

inline void
InitRNG32(pcg32_random_t *Rng, u64 Seed)
{
    pcg32_srandom_r(Rng, Seed, Seed^0x853c49e6748fea9bULL);
}

inline u32
RandomIndex(pcg32_random_t *Rng, u32 Count)
{
    Assert(Count > 0);
    u32 Result = pcg32_boundedrand_r(Rng, Count);

    return(Result);
}

#define EDITOR_RANDOM_H
#endif
