#if !defined(PLATFORM_SOUND_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): The engine writes samples on the main thread, the miniaudio
// callback reads them on the audio thread. Single producer, single consumer.
struct platform_sound_output
{
    b32 IsValid;
    ma_device Device;

    s32 SamplesPerSecond;
    s32 BytesPerSample;

    // NOTE(pvlso): In stereo frames, a power of two
    u32 RingSampleCount;
    s16 *RingSamples;
    u64 volatile WriteSampleIndex;
    u64 volatile ReadSampleIndex;

    u32 LatencySampleCount;
};

#define PLATFORM_SOUND_H
#endif
