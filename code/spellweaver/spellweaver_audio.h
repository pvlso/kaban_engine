#if !defined(SPELLWEAVER_AUDIO_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct playing_sound
{
    b32 Terminated;
    b32 SoundIsPlaying;
    
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;

    r32 dSample;

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

#define SPELLWEAVER_AUDIO_H
#endif
