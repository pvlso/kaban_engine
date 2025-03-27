#if !defined(EDITOR_AUDIO_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct playing_sound
{
    b32 Terminated;
    b32 SoundIsPlaying;
    
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;

    r32 dSample;

    loaded_sound *Sound;
    sound_id ID;
    r32 SamplesPlayed;
    playing_sound *Next;
};

struct audio_state
{
    memory_arena *PermArena;
    playing_sound *FirstPlayingSound;
    playing_sound *FirstFreePlayingSound;

    v2 MasterVolume;
};

#define EDITOR_AUDIO_H
#endif
