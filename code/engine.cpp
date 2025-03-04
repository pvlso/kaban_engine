/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>

#include "engine.h"
#include "engine_sort.cpp"
#include "engine_json_parser.cpp"
#include "engine_render_group.cpp"
#include "engine_asset.cpp"
#include "engine_audio.cpp"

//#include "engine_ui.cpp"
//#include "engine_triangle.cpp"
//#include "engine_triangle_f32.cpp"
//#include "engine_triangle_f64.cpp"

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

#if EDITOR_INTERNAL
internal u32
DEBUGGetMainGenerationID(editor_memory *Memory)
{
    u32 Result = 0;
    
    transient_state *TranState = Memory->TransientState;
    if(TranState)
    {
        Result = TranState->MainGenerationID;
    }

    return(Result);
}

internal editor_assets *
DEBUGGetEditorAssets(editor_memory *Memory)
{
    editor_assets *Assets = 0;
    
    transient_state *TranState = Memory->TransientState;
    if(TranState)
    {
        Assets = TranState->Assets;
    }

    return(Assets);
}
#endif

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

internal b32
CheckForMetaInput(editor_state *EditorState, transient_state *TranState, editor_input *Input)
{
    b32 Result = false;
    for(u32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        editor_controller_input *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsConnected)
        {
            if(WasPressed(Controller->Back))
            {
                switch(EditorState->EditorMode)
                {
                    case EditorMode_TitleScreen:
                    {
                        Input->QuitRequested = true;
                        break;
                    } break;
                }
            }
        }
    }

    if(WasPressed(Input->MouseButtons[PlatformMouseButton_Middle]))
    {
        EditorState->UIEnable = !EditorState->UIEnable;
    }

    return(Result);
}

inline working_version 
UpdateVersion(u32 Version)
{
    u8 MajorHigh = (Version >> 24) & 0xFF;
    u8 MajorLow = (Version >> 16) & 0xFF;
    u8 MinorHigh = (Version >> 8) & 0xFF;
    u8 MinorLow = Version & 0xFF;

    if((MinorLow + 1) == 255)
    {
        MinorLow = 0;
        if((MinorHigh + 1) == 255)
        {
            MinorHigh = 0;
            if((MajorLow + 1) == 255)
            {
                MajorLow = 0;
                if((MajorHigh + 1) == 255)
                {
                    MajorHigh = 0;
                }
                else
                {
                    MajorHigh += 1;
                }
            }
            else
            {
                MajorLow += 1;
            }
        }
        else
        {
            MinorHigh += 1;
        }
    }
    else
    {
        MinorLow += 1;
    }

    working_version Result = {MajorHigh, MajorLow, MinorHigh, MinorLow};//(u32)((MajorHigh << 24) | (MajorLow << 16) | (MinorHigh << 8) | MinorLow); 

    return(Result);
}

internal u32
UpdateEditorVersionFile(editor_state *EditorState)
{
    u32 Result = 0;
    
    FILE *VersionFile;
    fopen_s(&VersionFile, "editor_version_file.ssev", "rb");
    fread(&EditorState->Version, sizeof(working_version), 1, VersionFile);
    fclose(VersionFile);

    EditorState->Version = UpdateVersion((u32)((EditorState->Version.MajorHigh << 24) |
                                               (EditorState->Version.MajorLow << 16) |
                                               (EditorState->Version.MinorHigh << 8) |
                                               EditorState->Version.MinorLow));

    Result = (u32)((EditorState->Version.MajorHigh << 24) |
                   (EditorState->Version.MajorLow << 16) |
                   (EditorState->Version.MinorHigh << 8) |
                   EditorState->Version.MinorLow);
        
    fopen_s(&VersionFile, "editor_version_file.ssev", "wb");
    fwrite(&EditorState->Version, sizeof(working_version), 1, VersionFile);
    fclose(VersionFile);

    return(Result);
}

//#include "engine_title_mode.cpp"
//#include "engine_assets_mode.cpp"
//#include "engine_game_mode.cpp"

#if EDITOR_INTERNAL
debug_table *GlobalDebugTable;
editor_memory *DebugGlobalMemory;
#endif

platform_api Platform;
extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    GenerateCRC64Table();
    Platform = Memory->PlatformAPI;    
    
#if EDITOR_INTERNAL
    GlobalDebugTable = Memory->DebugTable;
    DebugGlobalMemory = Memory;
    
    {DEBUG_DATA_BLOCK("Renderer");
        {DEBUG_DATA_BLOCK("Camera");
            DEBUG_B32(Global_Renderer_Camera_UseDebug);
            DEBUG_VALUE(Global_Renderer_Camera_DebugDistance);
        }
    }

    {DEBUG_DATA_BLOCK("EditorGameMode");
        DEBUG_B32(Global_EditorGameMode_ShowCoords);
        DEBUG_B32(Global_EditorGameMode_ShowGrid);
    }
    
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

        FILE *VersionFile;
        fopen_s(&VersionFile, "editor_version_file.ssev", "rb");
        if(VersionFile)
        {
            fread(&EditorState->Version, sizeof(working_version), 1, VersionFile);
            fclose(VersionFile);
        }
        else
        {
            fopen_s(&VersionFile, "editor_version_file.ssev", "wb");
            fwrite(&EditorState->Version, sizeof(working_version), 1, VersionFile);
            fclose(VersionFile);
        }

        EditorState->MapStartup.MapVersion = EditorState->Version;
        EditorState->MapStartup.MapWidth = 48;
        EditorState->MapStartup.MapHeight = 48;
        EditorState->MapStartup.NewMap = true;
            
        EditorState->UIEnable = true;
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
//        PlayTitleScreen(EditorState, TranState);
    }
    
    //
    // NOTE(casey): Render
    //
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    // TODO: Decide what our pushbuffer size is!
    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID,
                                                 false, RenderCommands->Width, RenderCommands->Height);
    render_group *RenderGroup = &RenderGroup_;

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;

//    BeginUI(&EditorState->UIState, RenderCommands, TranState->Assets, TranState->MainGenerationID,
//            RenderWidth, RenderHeight);
    
//    char Buffer[128];
//    b32 Enabled = EditorState->UIEnable;
//    FormatString(ArrayCount(Buffer), Buffer, "UIEnabled: %s", Enabled ? "true" : "false");
//    UITextOutAt(&EditorState->UIState, V2(1070.0f, 703.0f), Buffer, 1.0f, Enabled ? V4(0, 1, 0, 1) : V4(1, 0, 0, 1), 4.0f);

    b32 Rerun = false;
    do
    {
        switch(EditorState->EditorMode)
        {
            case EditorMode_TitleScreen:
            {
//                Rerun = UpdateAndRenderTitleScreen(EditorState, TranState, RenderGroup, Input,
//                                                   RenderWidth, RenderHeight, EditorState->TitleScreen);
            } break;

            case EditorMode_AssetsMode:
            {
//                Rerun = UpdateAndRenderAssetsMode(EditorState, TranState, RenderGroup, Input,
//                                                   RenderWidth, RenderHeight, EditorState->AssetsMode);
            } break;

            case EditorMode_GameMode:
            {
//                Rerun = UpdateAndRenderGameMode(EditorState, TranState, RenderGroup, Input,
//                                                RenderWidth, RenderHeight, EditorState->GameMode);
            } break;

            InvalidDefaultCase;
        }
    } while(Rerun);
        
//    EndUI(EditorState, &EditorState->UIState, Input);

    EndRenderGroup(RenderGroup);

    EndTemporaryMemory(RenderMemory);
    
    CheckArena(&EditorState->ModeArena);
    CheckArena(&TranState->TranArena);
}

extern "C" EDITOR_GET_SOUND_SAMPLES(EditorGetSoundSamples)
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

