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
        Platform.OpenFile("..\\editor_metadata.json", PlatformFileType_None, PlatformFileOp_Write);

    char Data[256];
    FormatString(ArrayCount(Data), Data, "{\n    \"keas_version\": [%d, %d, %d, %d]\n}",
                 EditorMeta->KESAVersion[0], EditorMeta->KESAVersion[1],
                 EditorMeta->KESAVersion[2], EditorMeta->KESAVersion[3]);
    Platform.WriteDataToFile(&EditorMetaHandle, 0, StringLength(Data), Data);
    Platform.CloseFile(&EditorMetaHandle);

    return(Result);
}

#if EDITOR_INTERNAL
debug_table *GlobalDebugTable;
engine_memory *DebugGlobalMemory;
#endif

platform_api Platform;

#include "editor_title_mode.cpp"
#include "editor_assets_mode.cpp"
//#include "engine_navigation_mesh.cpp"
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
        EditorState->ArkhamCode = Platform.LoadCode("arkham.dll", "arkham_temp.dll", "lock.tmp");
        EditorState->Arkham =
            (arkham_update_and_render *)Platform.GetProcAddress(&EditorState->ArkhamCode,
                                                                "ArkhamUpdateAndRender");

        EditorState->EngineAPI.BeginRenderGroup = BeginRenderGroup;
        EditorState->EngineAPI.EndRenderGroup = EndRenderGroup;
        EditorState->EngineAPI.Perspective = Perspective;
        EditorState->EngineAPI.Orthographic = Orthographic;
        EditorState->EngineAPI.Clear = Clear;
        EditorState->EngineAPI.PushBitmap = PushBitmap;

        EditorState->EngineAPI.BeginGeneration = BeginGeneration;
        EditorState->EngineAPI.EndGeneration = EndGeneration;
        
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

        EditorState->EngineAPI.Assets = AllocateAssets(&TranState->TranArena, Megabytes(128),
                                                       TranState->LowPriorityQueue, &Memory->TextureOpQueue);
    }

    if(Memory->ExecutableReloaded)
    {
        Platform.UnloadCode(&EditorState->ArkhamCode);
        for(u32 LoadTryIndex = 0;
            LoadTryIndex < 100;
            ++LoadTryIndex)
        {
            EditorState->ArkhamCode = Platform.LoadCode("arkham.dll", "arkham_temp.dll", "lock.tmp");

            Platform.Sleep(100);

            EditorState->Arkham =
                (arkham_update_and_render *)Platform.GetProcAddress(&EditorState->ArkhamCode,
                                                                    "ArkhamUpdateAndRender");
            if(EditorState->Arkham)
                break;
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

    if(WasPressed(Input->Controllers[0].RightShoulder))
    {
        EditorState->UIEnable = !EditorState->UIEnable;        
    }

    u32 RenderWidth = RenderCommands->Width;
    u32 RenderHeight = RenderCommands->Height;
    BeginUI(&EditorState->UIState, RenderCommands, 0, 0, RenderWidth, RenderHeight, nk, UIScale);

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
                    EditorState->Arkham(EditorState->EngineAPI, RenderCommands, nk, RenderWidth, RenderHeight);
//                    Rerun = UpdateAndRenderMapEditor(EditorState, TranState, RenderGroup,
//                                                     Input, RenderWidth, RenderHeight);
                } break;

                InvalidDefaultCase;
            }
        } while(Rerun);
    }
    nk_end(nk);
    EndUI(EditorState, &EditorState->UIState, Input);
    
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

