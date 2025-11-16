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

internal u32
UpdateEditorVersionFile(editor_state *EditorState)
{
    u32 Result = 0;
    
    editor_meta *EditorMeta = &EditorState->EditorMeta;
    Result = *(u32 *)(EditorMeta->KESAVersion) + 1;
    EditorMeta->KESAVersion[0] = (Result >> 24) & 0xff;
    EditorMeta->KESAVersion[1] = (Result >> 16) & 0xff;
    EditorMeta->KESAVersion[2] = (Result >> 8)  & 0xff;
    EditorMeta->KESAVersion[3] = (Result)       & 0xff;
#if 0
    
    FILE *VersionFile;
    fopen_s(&VersionFile, "editor_version_file.ssev", "wb");
    fwrite(&EditorState->Version, sizeof(working_version), 1, VersionFile);
    fclose(VersionFile);
#endif
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
#include "engine_navigation_mesh.cpp"
//#include "engine_map_editor_mode.cpp"

internal void
EngineLoadEditorMetadata(editor_state *EditorState, char *MetadataSource)
{
    temporary_memory TempMem = BeginTemporaryMemory(&EditorState->TotalArena);
    json_object *Metadata = ParseJson(MetadataSource, TempMem.Arena, true);
    if(Metadata)
    {
        json_value *KEASVersion = JsonLookupObjectElement(Metadata, "keas_version");
        Assert(KEASVersion->Type == JsonValue_Array);

        for(u32 I = 0; I < KEASVersion->Array.Count; ++I)
        {
            EditorState->EditorMeta.KESAVersion[I] =
                (u8)KEASVersion->Array.Items[I]->Int;
        }
    }
    else
    {
        FILE *MetaFile;
        fopen_s(&MetaFile, MetadataSource, "wb");
        char Data[256];
        FormatString(ArrayCount(Data), Data, "{\n    \"keas_version\": [0, 0, 0, 0]\n}");
        fwrite(Data, StringLength(Data), 1, MetaFile);
        fclose(MetaFile);
    }
    
    EndTemporaryMemory(TempMem);
}

extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    
    nk_ui UI = Platform.UI;

    GenerateCRC64Table();
   
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

        EngineLoadEditorMetadata(EditorState, "..\\editor_metadata.json");

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
//        PlayMapEditor(EditorState, TranState);
        PlayTitleScreen(EditorState, TranState);
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

                case EditorMode_MapEditor:
                {
//                    Rerun = UpdateAndRenderMapEditor(EditorState, TranState, RenderGroup,
//                                                     Input, RenderWidth, RenderHeight);
                } break;

                case EditorMode_SimulateGame:
                {
                    EditorState->EditorMode = EditorMode_TitleScreen;
                    // TODO(pvlso): make the game as a saparate .dll 
//                    Rerun = GameUpdateAndRender(EditorState, TranState, Input, RenderCommands,
//                                                &Memory->TextureOpQueue);
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

