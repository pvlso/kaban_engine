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

#define NK_IMPLEMENTATION
#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#include "nuklear.h"

enum {EASY, HARD};
static int op = EASY;
static float value = 0.6f;
static int i =  20;
struct nk_context ctx;

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
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;
extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
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

        struct nk_font_atlas atlas = {};
        nk_font_atlas_init_default(&atlas);
        nk_font_atlas_begin(&atlas);
        nk_font *font = nk_font_atlas_add_from_file(&atlas, "C:\\Paul\\Spellweaver_Saga_game\\data\\editor\\fonts\\LiberationMono-Regular.ttf", 16, 0);
//        nk_font *font = nk_font_atlas_add_from_file(&atlas, "D:\\paul\\Spellweaver_Saga_game\\data\\editor\\fonts\\LiberationMono-Regular.ttf", 16, 0);
//                nk_font *font2 = nk_font_atlas_add_from_file(&atlas, "Path/To/Your/TTF_Font2.ttf", 16, 0);

        int width = 0;
        int height = 0;
        const void* img = nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32);
        nk_font_atlas_end(&atlas, nk_handle_id(0), 0);
 
        nk_size UIMemorySize = Megabytes(10);
        void *UIMemory = Platform.AllocateMemory(UIMemorySize);
                
        ctx = {};
        nk_init_fixed(&ctx, UIMemory, UIMemorySize, &font->handle);
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
//    PushRect(RenderGroup, &Default, V3(0, 0, 0.0f), V2(100.0f, 100.0f), V4(1, 0, 1, 1));

#if 1
    if (nk_begin(&ctx, "Show", nk_rect(50, 50, 220, 220),
                 NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE)) {
        // fixed widget pixel width
        nk_layout_row_static(&ctx, 30, 80, 1);
        if (nk_button_label(&ctx, "button")) {
            // event handling
        }
 
        // fixed widget window ratio width
        nk_layout_row_dynamic(&ctx, 30, 2);
        if (nk_option_label(&ctx, "easy", op == EASY)) op = EASY;
        if (nk_option_label(&ctx, "hard", op == HARD)) op = HARD;
 
        // custom widget pixel width
        nk_layout_row_begin(&ctx, NK_STATIC, 30, 2);
        {
            nk_layout_row_push(&ctx, 50);
            nk_label(&ctx, "Volume:", NK_TEXT_LEFT);
            nk_layout_row_push(&ctx, 110);
            nk_slider_float(&ctx, 0, &value, 1.0f, 0.1f);
        }
        nk_layout_row_end(&ctx);
    }
    nk_end(&ctx);

    const struct nk_command *cmd = 0;
    static s32 Counts[NK_COMMAND_CUSTOM] = {};
    f32 Z = 0.0f;
    u32 I = 0;
    nk_foreach(cmd, &ctx)
    {
        Z += 1.0f;
        I += 1;
        ++Counts[cmd->type];
        switch(cmd->type)
        {
            case NK_COMMAND_NOP:
            {
            } break;
            
            case NK_COMMAND_SCISSOR:
            {
                nk_command_scissor *S = (nk_command_scissor *)cmd;
                PushClipRect(RenderGroup, &Default,
                             V3((f32)S->x + 0.5f*(f32)S->w, -(f32)S->y - 0.5f*(f32)S->h, Z),
                             V2(S->w, S->h), 0);
            } break;
            
            case NK_COMMAND_LINE:
            {
                nk_command_line *L = (nk_command_line *)cmd;
            } break;
            
            case NK_COMMAND_CURVE:
            {
                nk_command_curve *C = (nk_command_curve *)cmd;
            } break;
            
            case NK_COMMAND_RECT:
            {
                nk_command_rect *R = (nk_command_rect *)cmd;
                PushRectOutline(RenderGroup, &Default,
                                V3((f32)R->x + 0.5f*(f32)R->w, -(f32)R->y - 0.5f*(f32)R->h, Z),
                                V2(R->w, R->h), V4(DebugColorTable[I], 1),
                                (f32)R->line_thickness);
            } break;
            
            case NK_COMMAND_RECT_FILLED:
            {
                nk_command_rect_filled *RF = (nk_command_rect_filled *)cmd;
                PushRect(RenderGroup, &Default,
                         V3((f32)RF->x + 0.5f*(f32)RF->w, -(f32)RF->y - 0.5f*(f32)RF->h, Z),
                         V2(RF->w, RF->h), V4(DebugColorTable[I], 1));

//                PushRect(RenderGroup, &Default, V3((f32)RF->x, -(f32)RF->y, Z),
//                         V2(RF->w, RF->h));
            } break;
            
            case NK_COMMAND_RECT_MULTI_COLOR:
            {
                nk_command_rect_multi_color *RM = (nk_command_rect_multi_color *)cmd;
            } break;
            
            case NK_COMMAND_CIRCLE:
            {
                nk_command_circle *C = (nk_command_circle *)cmd;
            } break;
            
            case NK_COMMAND_CIRCLE_FILLED:
            {
                nk_command_circle_filled *CF = (nk_command_circle_filled *)cmd;
            } break;
            
            case NK_COMMAND_ARC:
            {
                nk_command_arc *A = (nk_command_arc *)cmd;
            } break;
            
            case NK_COMMAND_ARC_FILLED:
            {
                nk_command_arc_filled *AF = (nk_command_arc_filled *)cmd;
            } break;
            
            case NK_COMMAND_TRIANGLE:
            {
                nk_command_triangle *T = (nk_command_triangle *)cmd;
            } break;
            
            case NK_COMMAND_TRIANGLE_FILLED:
            {
                nk_command_triangle_filled *TF = (nk_command_triangle_filled *)cmd;
            } break;
            
            case NK_COMMAND_POLYGON:
            {
                nk_command_polygon *P = (nk_command_polygon *)cmd;
            } break;
            
            case NK_COMMAND_POLYGON_FILLED:
            {
                nk_command_polygon_filled *PF = (nk_command_polygon_filled *)cmd;
            } break;
            
            case NK_COMMAND_POLYLINE:
            {
                nk_command_polyline *PL = (nk_command_polyline *)cmd;
            } break;
            
            case NK_COMMAND_TEXT:
            {
                nk_command_text *T = (nk_command_text *)cmd;
            } break;
            
            case NK_COMMAND_IMAGE:
            {
                nk_command_image *I = (nk_command_image *)cmd;
            } break;
            
            case NK_COMMAND_CUSTOM:
            {
            } break;

            default:
            {
            } break;
        }
    }
    nk_clear(&ctx);
#endif
    
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

