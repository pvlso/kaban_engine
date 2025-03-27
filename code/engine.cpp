/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine.h"
#include "engine_sort.cpp"
#include "engine_render_group.cpp"
#include "engine_asset.cpp"
#include "editor_audio.cpp"
//#include "editor_json_parser.cpp"

internal task_with_memory *
BeginTaskWithMemory(transient_state *TranState, b32 DependsOnEditorMode)
{
    task_with_memory *FoundTask = 0;

    for(uint32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        task_with_memory *Task = TranState->Tasks + TaskIndex;
        if(!Task->BeingUsed)
        {
            FoundTask = Task;
            Task->BeingUsed = true;
            Task->DependsOnEditorMode = DependsOnEditorMode;
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            break;
        }
    }

    return(FoundTask);
}

internal void
EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->MemoryFlush);

    CompletePreviousWritesBeforeFutureWrites;
    Task->BeingUsed = false;
}

internal void
SetEditorMode(editor_state *EditorState, transient_state *TranState, editor_mode EditorMode)
{
    b32 NeedToWait = false;
    for(u32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        NeedToWait = NeedToWait || TranState->Tasks[TaskIndex].DependsOnEditorMode;
    }
    if(NeedToWait)
    {
        Platform.CompleteAllWork(TranState->LowPriorityQueue);
    }

    Clear(&EditorState->ModeArena);
    EditorState->EditorMode = EditorMode;
}

#if EDITOR_INTERNAL
debug_table *GlobalDebugTable;
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;

#include "editor_title_mode.cpp"
//#include "editor_assets_mode.cpp"
//#include "editor_game_mode.cpp"

extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    

    nk_ui UI = Platform.UI;
    
#if EDITOR_INTERNAL
    GlobalDebugTable = Memory->DebugTable;
    DebugGlobalMemory = Memory;
    
    {DEBUG_DATA_BLOCK("Profile");
        DEBUG_UI_ELEMENT(DebugType_FrameSlider, FrameSlider);
        DEBUG_UI_ELEMENT(DebugType_LastFrameInfo, LastFrame);
        DEBUG_UI_ELEMENT(DebugType_DebugMemoryInfo, DebugMemory);
        DEBUG_UI_ELEMENT(DebugType_TopClocksList, EditorUpdateAndRender);
    }

#endif

    TIMED_FUNCTION();

    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));

    editor_state *EditorState = Memory->EditorState;
    if(!EditorState)
    {
        EditorState = Memory->EditorState = BootstrapPushStruct(editor_state, TotalArena);
        InitializeAudioState(&EditorState->AudioState, &EditorState->AudioArena);
    }

    // NOTE(casey): Transient initialization
    transient_state *TranState = Memory->TransientState;
    if(!TranState)
    {
        TranState = Memory->TransientState = BootstrapPushStruct(transient_state, TranArena);
            
        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;
        for(uint32 TaskIndex = 0;
            TaskIndex < ArrayCount(TranState->Tasks);
            ++TaskIndex)
        {
            task_with_memory *Task = TranState->Tasks + TaskIndex;
            Task->BeingUsed = false;
        }

        TranState->Assets = AllocateEditorAssets(&TranState->TranArena, Megabytes(512), TranState,
                                                 &Memory->TextureOpQueue);

    }

    {DEBUG_DATA_BLOCK("Memory");
        memory_arena *ModeArena = &EditorState->ModeArena;
        DEBUG_VALUE(ModeArena);
        
        memory_arena *AudioArena = &EditorState->AudioArena;
        DEBUG_VALUE(AudioArena);
        
        memory_arena *TranArena = &TranState->TranArena;
        DEBUG_VALUE(TranArena);
    }

    if(TranState->MainGenerationID)
    {
        EndGeneration(TranState->Assets, TranState->MainGenerationID);
    }

    TranState->MainGenerationID = BeginGeneration(TranState->Assets);

    if(EditorState->EditorMode == EditorMode_None)
    {
        PlayTitleScreen(EditorState, TranState);
    }
    
    //
    // NOTE(casey): Render
    //
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID,
                                                 false, RenderCommands->Width, RenderCommands->Height);
    render_group *RenderGroup = &RenderGroup_;
//    Clear(RenderGroup, V4(0.45f, 0, 0.45f, 1.0f));

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;
    
    if(UI.NkBegin(nk, "Title Screen", UI.NkRect(0, 0, (f32)RenderWidth, (f32)RenderHeight), 0))
    {
        b32 Rerun = false;
        do
        {
            switch(EditorState->EditorMode)
            {
                case EditorMode_None:
                {
                } break;

                case EditorMode_TitleScreen:
                {
                    Rerun = UpdateAndRenderTitleScreen(EditorState, TranState, nk, &UI, EditorState->TitleScreen);
                } break;
            
                case EditorMode_AssetsMode:
                {
                } break;

                case EditorMode_GameMode:
                {
                } break;

                InvalidDefaultCase;
            }
        } while(Rerun);
    }
    UI.NkEnd(nk);

    EndRenderGroup(RenderGroup);

    EndTemporaryMemory(RenderMemory);
    
    CheckArena(&EditorState->ModeArena);
    CheckArena(&TranState->TranArena);
}

extern "C" ENGINE_GET_SOUND_SAMPLES(EngineGetSoundSamples)
{
    editor_state *EditorState = Memory->EditorState;
    transient_state *TranState = Memory->TransientState;

    OutputPlayingSounds(&EditorState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
}

#if EDITOR_INTERNAL
#include "engine_debug.cpp"
#else
extern "C" DEBUG_EDITOR_FRAME_END(DEBUGEditorFrameEnd)
{
}
#endif

