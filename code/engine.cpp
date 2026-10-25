/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine.h"
#include "engine_sort.cpp"
#include "engine_json_parser.cpp"
#include "engine_render_group.cpp"
#include "engine_asset.cpp"
#include "engine_font.cpp"
#include "editor_audio.cpp"
#include "engine_ui.cpp"
#include "engine_math.cpp"

#include "polypartition.cpp"
#include "engine_poly_partition.cpp"

internal task_with_memory *
BeginTaskWithMemory(u32 TaskCount, task_with_memory *Tasks)
{
    task_with_memory *FoundTask = 0;

    for(uint32 TaskIndex = 0;
        TaskIndex < TaskCount;
        ++TaskIndex)
    {
        task_with_memory *Task = Tasks + TaskIndex;
        if(!Task->BeingUsed)
        {
            FoundTask = Task;
            Task->BeingUsed = true;
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
    TIMED_FUNCTION();

    b32 NeedToWait = false;
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    Clear(&EditorState->ModeArena);
    EditorState->EditorMode = EditorMode;

    char *ModeNames[] = {"None", "Title Screen", "Assets Mode", "Map Editor", "Game"};
    LOG("Editor mode: %s", (EditorMode < ArrayCount(ModeNames)) ? ModeNames[EditorMode] : "?");
}

// NOTE(pvlso): Next to every data/ directory (the engine's, each project's), holds the version
// of the stored assets (stored_assets_<version>.kesa) that the assets mode bumps on every write
#define EDITOR_METADATA_FILE_NAME "editor_metadata.json"

internal void
LoadEditorMetadata(editor_state *EditorState)
{
    ZeroStruct(EditorState->EditorMeta);

    temporary_memory TempMem = BeginTemporaryMemory(&EditorState->TotalArena);
    json_object *Metadata = ParseJson(EDITOR_METADATA_FILE_NAME, TempMem.Arena, PlatformFileType_DataRoot);
    if(Metadata)
    {
        json_value *KEASVersion = JsonLookupObjectElement(Metadata, "keas_version");
        Assert(KEASVersion->Type == JsonValue_Array);

        for(u32 I = 0; I < KEASVersion->Array.Count; ++I)
        {
            EditorState->EditorMeta.KESAVersion[I] =
                (u8)KEASVersion->Array.Items[I]->Int;
        }

        u8 *Version = EditorState->EditorMeta.KESAVersion;
        LOG("%s: stored assets version %d.%d.%d.%d", EDITOR_METADATA_FILE_NAME,
            Version[0], Version[1], Version[2], Version[3]);
    }
    else
    {
        LOG_WARNING("No %s, writing a new one (version 0)", EDITOR_METADATA_FILE_NAME);
        char Data[256];
        FormatString(ArrayCount(Data), Data, "{\n    \"keas_version\": [0, 0, 0, 0]\n}");
        Platform.WriteEntireFile(EDITOR_METADATA_FILE_NAME, PlatformFileType_DataRoot, (u8 *)Data, StringLength(Data));
    }

    EndTemporaryMemory(TempMem);
}

// NOTE(pvlso): Points every data file at a project (projects/<name>/data/ and its editor_metadata.json),
// or at the engine's data/ when ProjectName is 0. The assets mode and a running game use their
// project's data, the title screen goes back to the engine's.
internal b32
UseDataOf(editor_state *EditorState, char *ProjectName)
{
    b32 Result = Platform.SetDataProject(ProjectName);
    if(Result)
    {
        LoadEditorMetadata(EditorState);
    }

    return(Result);
}

internal u32
UpdateEditorVersionFile(editor_state *EditorState)
{
    u32 Result = 0;
    
    editor_meta *EditorMeta = &EditorState->EditorMeta;

    u32 FullVersion = (((u32)EditorMeta->KESAVersion[0] << 24) |
                       ((u32)EditorMeta->KESAVersion[1] << 16) |
                       ((u32)EditorMeta->KESAVersion[2] <<  8) |
                       ((u32)EditorMeta->KESAVersion[3]));

    Result = FullVersion + 1;
    EditorMeta->KESAVersion[0] = (Result >> 24) & 0xff;
    EditorMeta->KESAVersion[1] = (Result >> 16) & 0xff;
    EditorMeta->KESAVersion[2] = (Result >> 8)  & 0xff;
    EditorMeta->KESAVersion[3] = (Result)       & 0xff;

    platform_file_handle EditorMetaHandle =
        Platform.OpenFile(EDITOR_METADATA_FILE_NAME, PlatformFileType_DataRoot, PlatformFileOp_Write);

    char Data[256];
    FormatString(ArrayCount(Data), Data, "{\n    \"keas_version\": [%d, %d, %d, %d]\n}",
                 EditorMeta->KESAVersion[0], EditorMeta->KESAVersion[1],
                 EditorMeta->KESAVersion[2], EditorMeta->KESAVersion[3]);
    Platform.WriteDataToFile(&EditorMetaHandle, 0, StringLength(Data), Data);
    Platform.CloseFile(&EditorMetaHandle);
    LOG("Stored assets version is now %u", Result);

    return(Result);
}

#if EDITOR_INTERNAL
debug_table *GlobalDebugTable;
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;

#include "editor_title_mode.cpp"
#include "editor_assets_mode.cpp"
#include "editor_map_mode.cpp"
#include "editor_game_mode.cpp"
//#include "engine_navigation_mesh.cpp"
//#include "engine_map_editor_mode.cpp"

extern "C" ENGINE_UPDATE_AND_RENDER(EngineUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    
    GenerateCRC64Table();
   
#if EDITOR_INTERNAL
    GlobalDebugTable = Memory->DebugTable;
    DebugGlobalMemory = Memory;
#endif

    TIMED_FUNCTION();

    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));

    editor_state *EditorState = Memory->EditorState;
    if(!EditorState)
    {
        EditorState = Memory->EditorState = BootstrapPushStruct(editor_state, TotalArena);
        InitializeAudioState(&EditorState->AudioState, &EditorState->AudioArena);

        LoadEditorMetadata(EditorState);
        
        EditorState->UIEnable = true;
        LOG("Editor state initialized");
    }

    // NOTE(casey): Transient initialization
    transient_state *TranState = Memory->TransientState;
    if(!TranState)
    {
        TranState = Memory->TransientState = BootstrapPushStruct(transient_state, TranArena);
            
        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;
        TranState->TextureOpQueue = &Memory->TextureOpQueue;

        for(uint32 TaskIndex = 0;
            TaskIndex < ArrayCount(TranState->Tasks);
            ++TaskIndex)
        {
            task_with_memory *Task = TranState->Tasks + TaskIndex;
            Task->BeingUsed = false;
        }
    }
    
    if(EditorState->EditorMode == EditorMode_None)
    {
        PlayTitleScreen(EditorState, TranState);
    }
    
    if(EditorState->EditorMode == EditorMode_TitleScreen)
    {
        nk->style.window.fixed_background.data.color.a = 0;
    }
    else
    {
        nk->style.window.fixed_background.data.color.a = 0;
    }
    
    //
    // NOTE(casey): Render
    //

    // NOTE(pvlso): Alt + E toggles the UI, Alt + M mutes / unmutes the audio
    if(Input->AltDown && WasPressed(Input->Controllers[0].RightShoulder))
    {
        EditorState->UIEnable = !EditorState->UIEnable;        
    }

    if(Input->AltDown && WasPressed(Input->Controllers[0].ToggleMute))
    {
        EditorState->AudioState.Muted = !EditorState->AudioState.Muted;
        LOG("Audio %s", EditorState->AudioState.Muted ? "muted" : "unmuted");
    }

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;
    BEGIN_BLOCK("BeginUI");
    BeginUI(&EditorState->UIState, RenderCommands, 0, 0, RenderWidth, RenderHeight, nk, UIScale);
    END_BLOCK();

    if(nk_begin(nk, "UI Window", nk_rect(0, 0, (f32)1920, (f32)1080),
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
                    Rerun = UpdateAndRenderAssetsMode(EditorState, TranState,
                                                      Input, &Memory->TextureOpQueue);
                } break;

                case EditorMode_MapEditor:
                {
                    Rerun = UpdateAndRenderMapEditor(EditorState, TranState, Input, &Memory->TextureOpQueue);
                } break;

                case EditorMode_Game:
                {
                    Rerun = UpdateAndRenderGame(EditorState, TranState, Input, RenderCommands);
                } break;

                InvalidDefaultCase;
            }
        } while(Rerun);
    }
    nk_end(nk);

    BEGIN_BLOCK("EndUI");
    EndUI(EditorState, &EditorState->UIState, Input);
    END_BLOCK();
    
    CheckArena(&EditorState->ModeArena);
    CheckArena(&TranState->TranArena);
}

extern "C" ENGINE_GET_SOUND_SAMPLES(EngineGetSoundSamples)
{
    editor_state *EditorState = Memory->EditorState;
    transient_state *TranState = Memory->TransientState;

//    OutputPlayingSounds(&EditorState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
}

#if EDITOR_INTERNAL
#include "engine_debug.cpp"
#else
extern "C" DEBUG_EDITOR_FRAME_END(DEBUGEditorFrameEnd)
{
}
#endif

