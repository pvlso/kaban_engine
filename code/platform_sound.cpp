/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

/*
  NOTE(pvlso): Sound output through miniaudio, the same on every OS.
*/

internal void
PlatformAudioCallback(ma_device *Device, void *Output, const void *Input, ma_uint32 FrameCount)
{
    platform_sound_output *SoundOutput = (platform_sound_output *)Device->pUserData;
    s16 *Dest = (s16 *)Output;

    u64 ReadSampleIndex = SoundOutput->ReadSampleIndex;
    u64 WriteSampleIndex = SoundOutput->WriteSampleIndex;
    CompletePreviousReadsBeforeFutureReads;

    u64 Available = WriteSampleIndex - ReadSampleIndex;
    u32 CopyCount = (Available < FrameCount) ? (u32)Available : FrameCount;
    u32 RingMask = SoundOutput->RingSampleCount - 1;

    for(u32 SampleIndex = 0;
        SampleIndex < CopyCount;
        ++SampleIndex)
    {
        u32 RingIndex = (u32)((ReadSampleIndex + SampleIndex) & RingMask);
        *Dest++ = SoundOutput->RingSamples[2*RingIndex + 0];
        *Dest++ = SoundOutput->RingSamples[2*RingIndex + 1];
    }

    // NOTE(pvlso): Underrun, play silence until the main thread catches up
    for(u32 SampleIndex = CopyCount;
        SampleIndex < FrameCount;
        ++SampleIndex)
    {
        *Dest++ = 0;
        *Dest++ = 0;
    }

    CompletePreviousReadsBeforeFutureReads;
    CompletePreviousWritesBeforeFutureWrites;
    SoundOutput->ReadSampleIndex = ReadSampleIndex + CopyCount;
}

internal void
PlatformInitSound(platform_sound_output *SoundOutput, s32 SamplesPerSecond, u32 LatencySampleCount)
{
    SoundOutput->SamplesPerSecond = SamplesPerSecond;
    SoundOutput->BytesPerSample = sizeof(s16)*2;
    SoundOutput->RingSampleCount = 65536;
    SoundOutput->LatencySampleCount = LatencySampleCount;
    SoundOutput->RingSamples = (s16 *)OSAllocateMemory(SoundOutput->RingSampleCount*SoundOutput->BytesPerSample);
    SoundOutput->ReadSampleIndex = 0;
    SoundOutput->WriteSampleIndex = 0;

    Assert(LatencySampleCount < SoundOutput->RingSampleCount);

    ma_device_config Config = ma_device_config_init(ma_device_type_playback);
    Config.playback.format = ma_format_s16;
    Config.playback.channels = 2;
    Config.sampleRate = SamplesPerSecond;
    Config.dataCallback = PlatformAudioCallback;
    Config.pUserData = SoundOutput;

    if(SoundOutput->RingSamples &&
       (ma_device_init(0, &Config, &SoundOutput->Device) == MA_SUCCESS))
    {
        if(ma_device_start(&SoundOutput->Device) == MA_SUCCESS)
        {
            SoundOutput->IsValid = true;
        }
        else
        {
            ma_device_uninit(&SoundOutput->Device);
        }
    }

    if(!SoundOutput->IsValid)
    {
        fprintf(stderr, "PLATFORM: Failed to start audio device, running without sound.\n");
    }
}

internal void
PlatformShutdownSound(platform_sound_output *SoundOutput)
{
    if(SoundOutput->IsValid)
    {
        ma_device_uninit(&SoundOutput->Device);
        SoundOutput->IsValid = false;
    }
}

// NOTE(pvlso): Keeps LatencySampleCount samples queued ahead of the audio thread
internal void
PlatformUpdateSound(platform_sound_output *SoundOutput, platform_engine_code *Engine, engine_memory *Memory, s16 *Samples)
{
    if(SoundOutput->IsValid)
    {
        u64 ReadSampleIndex = SoundOutput->ReadSampleIndex;
        u64 WriteSampleIndex = SoundOutput->WriteSampleIndex;
        CompletePreviousReadsBeforeFutureReads;

        u32 QueuedSampleCount = (u32)(WriteSampleIndex - ReadSampleIndex);
        if(QueuedSampleCount < SoundOutput->LatencySampleCount)
        {
            u32 FreeSampleCount = (SoundOutput->RingSampleCount - QueuedSampleCount) & ~7;

            engine_sound_output_buffer SoundBuffer = {};
            SoundBuffer.SamplesPerSecond = SoundOutput->SamplesPerSecond;
            SoundBuffer.SampleCount = Align8(SoundOutput->LatencySampleCount - QueuedSampleCount);
            if((u32)SoundBuffer.SampleCount > FreeSampleCount)
            {
                SoundBuffer.SampleCount = FreeSampleCount;
            }
            SoundBuffer.Samples = Samples;

            if(Engine->GetSoundSamples)
            {
                Engine->GetSoundSamples(Memory, &SoundBuffer);
            }
            else
            {
                ZeroSize(SoundBuffer.SampleCount*SoundOutput->BytesPerSample, Samples);
            }

            u32 RingMask = SoundOutput->RingSampleCount - 1;
            s16 *Source = Samples;
            for(s32 SampleIndex = 0;
                SampleIndex < SoundBuffer.SampleCount;
                ++SampleIndex)
            {
                u32 RingIndex = (u32)((WriteSampleIndex + SampleIndex) & RingMask);
                SoundOutput->RingSamples[2*RingIndex + 0] = *Source++;
                SoundOutput->RingSamples[2*RingIndex + 1] = *Source++;
            }

            CompletePreviousWritesBeforeFutureWrites;
            SoundOutput->WriteSampleIndex = WriteSampleIndex + SoundBuffer.SampleCount;
        }
    }
}
