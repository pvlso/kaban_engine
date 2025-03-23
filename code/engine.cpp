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
DEBUGGetMainGenerationID(engine_memory *Memory)
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
DEBUGGetEditorAssets(engine_memory *Memory)
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
CheckForMetaInput(editor_state *EditorState, transient_state *TranState, engine_input *Input)
{
    b32 Result = false;
    for(u32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        engine_controller_input *Controller = GetController(Input, ControllerIndex);
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
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;

extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    

    nk_ui UI = Platform.UI;
    
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
        char window_title[64] = "Title";
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
    Clear(RenderGroup, V4(0.45f, 0, 0.45f, 1.0f));

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;

    Orthographic(RenderGroup, 2.0f);

    object_transform Default = DefaultFlatTransform();
    Default.OffsetP = V3(-100.0f, 100.0f, 0.0f);

    if (NkTreePush(nk, NK_TREE_TAB, "Tree", NK_MINIMIZED)) {
        UI.NkLayoutRowDynamic(nk, 30, 4);

        if (UI.NkButtonLabel(nk, "Make Windowed"))
        {
        }

        UI.NkTreePop(nk);
    }
    
//    PushRect(RenderGroup, &Default, V3(0, 0, 0.0f), V2(100.0f, 100.0f), V4(1, 0, 1, 1));
    if (UI.NkBegin(nk, "Demo", UI.NkRect(50, 50, 230, 250),
                            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        UI.NkLayoutRowDynamic(nk, 30, 4);

        if (UI.NkButtonLabel(nk, "Make Windowed"))
        {
        }

        if (UI.NkButtonLabel(nk, "Maximize"))
        {
        }
        if (UI.NkButtonLabel(nk, "Iconify"))
        {
        }
        if (UI.NkButtonLabel(nk, "Restore"))
        {
        }

        UI.NkLayoutRowDynamic(nk, 30, 2);

        if (UI.NkButtonLabel(nk, "Hide (for 3s)"))
        {
        }

        if (UI.NkButtonLabel(nk, "Request Attention (after 3s)"))
        {
        }

        UI.NkLayoutRowDynamic(nk, 30, 1);

        UI.NkLabel(nk, "Press Enter in a text field to set value", NK_TEXT_CENTERED);

        nk_flags events;
        const nk_flags flags = NK_EDIT_FIELD |
            NK_EDIT_SIG_ENTER |
            NK_EDIT_GOTO_END_ON_ACTIVATE;

        UI.NkLayoutRowBegin(nk, NK_DYNAMIC, 30, 2);
        UI.NkLayoutRowPush(nk, 1.f / 3.f);
        UI.NkLabel(nk, "Title", NK_TEXT_LEFT);
        UI.NkLayoutRowPush(nk, 2.f / 3.f);
        events = UI.NkEditStringZeroTerminated(nk, flags, EditorState->window_title,
                                               sizeof(EditorState->window_title), NULL);
        if (events & NK_EDIT_COMMITED)
        {
        }

        UI.NkLayoutRowEnd(nk);
        UI.NkLabel(nk, "Platform does not support window position", NK_TEXT_LEFT);

        UI.NkLayoutRowDynamic(nk, 30, 3);
        UI.NkLabel(nk, "Size", NK_TEXT_LEFT);

        UI.NkLabel(nk, "Framebuffer Size", NK_TEXT_LEFT);
        UI.NkLabelf(nk, NK_TEXT_LEFT, "%i", RenderCommands->Width);
        UI.NkLabelf(nk, NK_TEXT_LEFT, "%i", RenderCommands->Height);
    }
    UI.NkEnd(nk);
    
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

