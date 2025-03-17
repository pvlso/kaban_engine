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
//        EditorState->UIEnable = !EditorState->UIEnable;
    }

    return(Result);
}

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
    
    //
    // NOTE(casey): Render
    //
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID,
                                                 false, RenderCommands->Width, RenderCommands->Height);
    render_group *RenderGroup = &RenderGroup_;

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;

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

    EndRenderGroup(RenderGroup);

    EndTemporaryMemory(RenderMemory);
    
    CheckArena(&EditorState->ModeArena);
    CheckArena(&TranState->TranArena);
}

#if EDITOR_INTERNAL
#include "engine_debug.cpp"
#else
extern "C" DEBUG_EDITOR_FRAME_END(DEBUGEditorFrameEnd)
{
}
#endif

