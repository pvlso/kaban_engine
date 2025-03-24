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

struct nk_color colors[] = {
    {255, 100, 100, 255}, // Reddish
    {100, 255, 100, 255}, // Reddish
    {100, 100, 255, 255}, // Reddish
};

static float data[3][5] = {
    {10.0f, 15.0f, 20.0f, 25.0f, 30.0f}, // Dataset 1
    {5.0f, 10.0f, 15.0f, 10.0f, 5.0f},   // Dataset 2
    {8.0f, 5.0f, 10.0f, 15.0f, 20.0f}    // Dataset 3
};
const int dataset_count = 3;
const int point_count = 5;
const float max_value = 60.0f; // Max cumulative value for scaling

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

#if 0
    static float values[] = {1.0f, 2.5f, 1.8f, 3.2f, 2.0f};
    static float values0[] = {4.8f, 4.0f, 4.5f, 4.2f, 4.0f};
    static int value_count = sizeof(values) / sizeof(values[0]);
    if (UI.NkBegin(nk, "Profiler", UI.NkRect(400, 0, 230, 250),
                   NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                   NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE))
    {
        UI.NkLayoutRowDynamic(nk, 200, 1);
        if (UI.NkChartBeginColored(nk, NK_CHART_COLUMN, {0, 0, 255, 255}, {255, 0, 0, 255}, value_count, 0.0f, 5.0f))
        {
            for (int i = 0; i < value_count; i++) {
                UI.NkChartPush(nk, values[i]);
            }

            UI.NkChartAddSlotColored(nk, NK_CHART_COLUMN, {0, 255, 0, 255}, {255, 0, 255, 255}, value_count, 0.0f, 5.0f);
            for (int i = 0; i < value_count; i++) {
                UI.NkChartPushSlot(nk, values0[i], 1);
            }

            UI.NkChartEnd(nk);
        }        
    }
    UI.NkEnd(nk);
#endif

    // Begin Nuklear window
    if(UI.NkBegin(nk, "Stacked Chart Demo", UI.NkRect(50, 50, 700, 500),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE)) {
        // Reserve space for the chart
        UI.NkLayoutRowDynamic(nk, 400, 1); // 400px height
        struct nk_rect chart_bounds = UI.NkWidgetBounds(nk);

        // Chart dimensions
        float chart_width = chart_bounds.w;
        float chart_height = chart_bounds.h;
        float bar_width = chart_width / point_count;
        float scale = chart_height / max_value; // Pixels per unit

        // Draw stacked bars
        struct nk_command_buffer* canvas = UI.NkWindowGetCanvas(nk);
        for (int i = 0; i < point_count; i++) {

            float x = chart_bounds.x + i * bar_width;
            float y_base = chart_bounds.y + chart_height; // Start from bottom

            // Stack each dataset
            for (int j = 0; j < dataset_count; j++) {
                float height = data[j][i] * scale;
                struct nk_rect bar = UI.NkRect(x, y_base - height, bar_width - 2, height); // -2 for spacing
                UI.NkFillRect(canvas, bar, 0, colors[j]);
                y_base -= height; // Move up for next stack
            }
        }

        // Optional: Add labels or legend (simple text for now)
        UI.NkLayoutRowDynamic(nk, 20, 1);
        UI.NkLabel(nk, "Stacked Bar Chart (3 Datasets)", NK_TEXT_CENTERED);
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

