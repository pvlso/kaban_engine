/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
// TODO(paul): Use file API for this
#include <stdio.h>

#include "engine.h"
#include "engine_sort.cpp"
#include "engine_json_parser.cpp"
#include "engine_render_group.cpp"
#include "engine_asset.cpp"
#include "editor_audio.cpp"
#include "engine_ui.cpp"
#include "engine_math.cpp"
#include "polypartition.cpp"

#include "engine_poly_partition.cpp"

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
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    Clear(&EditorState->ModeArena);
    EditorState->EditorMode = EditorMode;
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

    working_version Result = {MajorHigh, MajorLow, MinorHigh, MinorLow};

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

#if EDITOR_INTERNAL
debug_table *GlobalDebugTable;
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;

#include "editor_title_mode.cpp"
#include "editor_assets_mode.cpp"
#include "engine_game_simulate.cpp"
#include "engine_map_editor_mode.cpp"

extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    
    nk_ui UI = Platform.UI;

    GenerateCRC64Table();

#if 0
    TPPLPartition Partition = TPPLPartition();

    TPPLPolyList PolyList;

    TPPLPoly TPoly = {};
    TPoly.Init(4);
    TPoly.points[0] = {0.0, 0.0};
    TPoly.points[1] = {0.0, 4.0};
    TPoly.points[2] = {4.0, 4.0};
    TPoly.points[3] = {4.0, 0.0};
    TPoly.SetOrientation(TPPL_ORIENTATION_CCW);
    PolyList.push_back(TPoly);

    TPPLPoly TPoly0 = {};
    TPoly0.Init(4);
    TPoly0.points[0] = {0.0, 0.0};
    TPoly0.points[1] = {0.0, 2.0};
    TPoly0.points[2] = {2.0, 2.0};
    TPoly0.points[3] = {2.0, 0.0};
    TPoly0.hole = true;
    TPoly0.SetOrientation(TPPL_ORIENTATION_CW);
    PolyList.push_back(TPoly0);

    TPPLPolyList List;
    s32 Result = Partition.RemoveHoles(&PolyList, &List);

    TPPLPolyList Res;
    s32 R = Partition.ConvexPartition_HM(&PolyList, &Res);
#endif
    
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
        
        FILE *VersionFile;
        fopen_s(&VersionFile, "editor_version_file.ssev", "rb");
        if(VersionFile)
        {
            fread(&EditorState->Version, 4, 1, VersionFile);
            fclose(VersionFile);
        }
        else
        {
            fopen_s(&VersionFile, "editor_version_file.ssev", "wb");
            EditorState->Version.MinorLow = 63;
            fwrite(&EditorState->Version, 4, 1, VersionFile);
            fclose(VersionFile);
        }

        EditorState->MapStartup.MapVersion = EditorState->Version;
        EditorState->MapStartup.MapWidth = 48;
        EditorState->MapStartup.MapHeight = 48;
        EditorState->MapStartup.NewMap = false;

        EditorState->UIEnable = true;

        TPPLPartition partition;
        TPPLPolyList inpolys; // Input polygons (outer polygon + hole)
        TPPLPolyList convexParts; // Output triangles

        // NOTE(paul): RULES
        // ALL POINTS OF THE HOLE POLYGON HAS TO BE INSIDE THE OUTER POLY
        // OUTER POLY HAS TO BE COUNTER-CLOCKWISE
        // INNER POLY HAS TO BE CLOCKWISE
    
        // Define the outer polygon (square, counter-clockwise)
        TPPLPoly outer;
        outer.Init(4);
        outer.points[0] = {0, 0}; // Bottom-left
        outer.points[1] = {4, 0}; // Bottom-right
        outer.points[2] = {4, 4}; // Top-right
        outer.points[3] = {0, 4}; // Top-left
        outer.SetOrientation(TPPL_ORIENTATION_CCW); // Ensure counter-clockwise

        // Define the hole (smaller square, clockwise)
        TPPLPoly hole;
        hole.Init(4);
        hole.points[0] = {1, 1}; // Bottom-left
        hole.points[1] = {1, 3}; // Top-left
        hole.points[2] = {3, 3}; // Top-right
        hole.points[3] = {3, 1}; // Bottom-right
        hole.SetOrientation(TPPL_ORIENTATION_CW); // Ensure clockwise
        hole.SetHole(true); // Mark as a hole

        // Add both polygons to the input list
        inpolys.push_back(outer);
        inpolys.push_back(hole);

        // Triangulate the polygon with the hole
//    int result = partition.Triangulate_EC(&inpolys, &triangles);
        int result = partition.ConvexPartition_HM(&inpolys, &convexParts);
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
        PlayGameMode(EditorState, TranState);
//        PlayTitleScreen(EditorState, TranState);
    }

    if(EditorState->SimulationQuit)
    {
        PlayTitleScreen(EditorState, TranState);
        EditorState->SimulationQuit = false;
    }
    
    if(EditorState->EditorMode == EditorMode_TitleScreen)
    {
        nk->style.window.fixed_background.data.color.a = 255;
    }
    else
    {
        nk->style.window.fixed_background.data.color.a = 0;
    }
    
    //
    // NOTE(casey): Render
    //
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID,
                                                 false, RenderCommands->Width, RenderCommands->Height);
    render_group *RenderGroup = &RenderGroup_;
    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;
    
    if(WasPressed(Input->Controllers[0].RightShoulder))
    {
        EditorState->UIEnable = !EditorState->UIEnable;        
    }

    BeginUI(&EditorState->UIState, RenderCommands, TranState->Assets, TranState->MainGenerationID,
            RenderWidth, RenderHeight, &UI, nk);
    if(UI.NkBegin(nk, "UI Window", UI.NkRect(0, 0, (f32)nk->BaseWidth, (f32)nk->BaseHeight),
                  (!EditorState->UIEnable) ? NK_WINDOW_NOT_INTERACTIVE|NK_WINDOW_NO_SCROLLBAR : NK_WINDOW_REMOVE_ROM|NK_WINDOW_NO_SCROLLBAR))
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
                    Rerun = UpdateAndRenderTitleScreen(EditorState, TranState);
                } break;
            
                case EditorMode_AssetsMode:
                {
                    Rerun = UpdateAndRenderAssetsMode(EditorState, TranState, Input);
                } break;

                case EditorMode_GameMode:
                {
                    Rerun = UpdateAndRenderGameMode(EditorState, TranState, RenderGroup,
                                                    Input, RenderWidth, RenderHeight);
                } break;

                case EditorMode_SimulateGame:
                {
                    Rerun = GameUpdateAndRender(EditorState, TranState, Input, RenderCommands,
                                                &Memory->TextureOpQueue);
                } break;

                InvalidDefaultCase;
            }
        } while(Rerun);
    }
    UI.NkEnd(nk);
    EndUI(EditorState, &EditorState->UIState, Input);
    
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

