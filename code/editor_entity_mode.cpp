/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): The entity editor: see editor_entity_mode.h

// NOTE(pvlso): The definitions in data/entities, with their GUIDs (each file read once)
internal void
ListEntityFiles(editor_mode_entity *EntityMode)
{
    EntityMode->EntityFileCount = Platform.ListFilesInDirectory(PlatformFileType_Entity, 0, 0);
    EntityMode->EntityFiles = PushArray(EntityMode->Arena, Maximum(1, EntityMode->EntityFileCount), char *);
    EntityMode->EntityGUIDs = PushArray(EntityMode->Arena, Maximum(1, EntityMode->EntityFileCount), u64);
    Platform.ListFilesInDirectory(PlatformFileType_Entity, EntityMode->EntityFiles, EntityMode->Arena);
    for(u32 Index = 0; Index < EntityMode->EntityFileCount; ++Index)
    {
        entity_file Entity = ReadEntityFile(EntityMode->EntityFiles[Index], EntityMode->Project, EntityMode->Arena);
        EntityMode->EntityGUIDs[Index] = Entity.GUID;
    }
    EntityMode->EntityFileIndex = 0;
}

// NOTE(pvlso): The file name of the definition with that GUID, 0 when none has it
internal char *
EntityFileOfGUID(editor_mode_entity *EntityMode, u64 GUID)
{
    char *Result = 0;
    for(u32 Index = 0; GUID && !Result && (Index < EntityMode->EntityFileCount); ++Index)
    {
        if(EntityMode->EntityGUIDs[Index] == GUID) {Result = EntityMode->EntityFiles[Index];}
    }

    return(Result);
}

// NOTE(pvlso): The project's stored bitmaps and sprite sheets, what a clip can look like
internal void
ListLookChoices(editor_state *EditorState, editor_mode_entity *EntityMode)
{
    kesa_header Header = {};
    kesa_asset *Stored = ReadStoredAssetList(&EditorState->EditorMeta, &Header);
    u32 Count = 0;
    for(u32 AssetIndex = 1; Stored && (AssetIndex < Header.AssetCount); ++AssetIndex)
    {
        kesa_type Type = Stored[AssetIndex].Type;
        if((Type == KESA_Bitmap) || (Type == KESA_SpriteSheet)) {++Count;}
    }

    EntityMode->LookChoiceCount = Count;
    EntityMode->LookChoices = PushArray(EntityMode->Arena, Maximum(1, Count), entity_look_choice);
    EntityMode->LookLabels = PushArray(EntityMode->Arena, Maximum(1, Count), char *);
    u32 ChoiceIndex = 0;
    for(u32 AssetIndex = 1; Stored && (AssetIndex < Header.AssetCount); ++AssetIndex)
    {
        kesa_asset *Asset = Stored + AssetIndex;
        if((Asset->Type != KESA_Bitmap) && (Asset->Type != KESA_SpriteSheet))
        {
            continue;
        }

        entity_look_choice *Choice = EntityMode->LookChoices + ChoiceIndex;
        FormatString(ENTITY_LOOK_NAME_LENGTH, Choice->FileName, "%s", Asset->SourceFileName);
        if(Asset->Type == KESA_Bitmap)
        {
            Choice->Kind = EntityLook_Bitmap;
            Choice->Align = Asset->Bitmap.AlignPercentage;
        }
        else
        {
            Choice->Kind = EntityLook_SpriteSheet;
            Choice->Align = Asset->SpriteSheet.SpriteAlignPercentage;
            Choice->SpriteCount = Asset->SpriteSheet.SpriteCount;
            Choice->SpriteWidth = Asset->SpriteSheet.SpriteWidth;
            Choice->SpriteHeight = Asset->SpriteSheet.SpriteHeight;
        }

        char Label[ENTITY_LOOK_NAME_LENGTH + 16];
        FormatString(sizeof(Label), Label, "%s  %s", (Asset->Type == KESA_Bitmap) ? "bitmap" : "animation",
                     Asset->SourceFileName);
        EntityMode->LookLabels[ChoiceIndex] = PushString(EntityMode->Arena, Label);
        ++ChoiceIndex;
    }
    Platform.DeallocateMemory(Stored);
    LOG("Entity editor: %u looks among the stored assets", Count);
}

internal s32
FindLookChoice(editor_mode_entity *EntityMode, entity_clip *Clip)
{
    s32 Result = -1;
    for(u32 ChoiceIndex = 0; (Result < 0) && (ChoiceIndex < EntityMode->LookChoiceCount); ++ChoiceIndex)
    {
        entity_look_choice *Choice = EntityMode->LookChoices + ChoiceIndex;
        if((Choice->Kind == Clip->Kind) && StringsAreEqual(Choice->FileName, Clip->FileName)) {Result = (s32)ChoiceIndex;}
    }

    return(Result);
}

internal void
PlayEntityEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_EntityEditor);
    LOG("Entity Editor for %s", Name);
    UseDataOf(EditorState, Name);

    editor_mode_entity *EntityMode = PushStruct(&EditorState->ModeArena, editor_mode_entity);
    FormatString(sizeof(EntityMode->ProjectName), EntityMode->ProjectName, "%s", Name);
    EntityMode->Arena = &EditorState->ModeArena;
    EntityMode->Zoom = 4.0f;
    EntityMode->PreviewChoice = -1;
    EntityMode->ShowFootprint = true;
    EntityMode->ShowHitArea = true;
    EntityMode->Project = ReadEntityProject(EntityMode->Arena);
    ListLookChoices(EditorState, EntityMode);
    ListEntityFiles(EntityMode);

    EditorState->EntityMode = EntityMode;
}

internal void
ReleaseEntityPreview(platform_texture_op_queue *TextureOpQueue, editor_mode_entity *EntityMode)
{
    DeallocateBitmap(TextureOpQueue, &EntityMode->Preview);
    EntityMode->Preview = {};
    EntityMode->PreviewClip = {};
    EntityMode->PreviewChoice = -1;
}

// NOTE(pvlso): The shown clip's look loaded, when it isn't the one loaded already
internal void
UpdateEntityPreview(platform_texture_op_queue *TextureOpQueue, editor_mode_entity *EntityMode)
{
    entity_clip *Clip = EntityMode->Entity.Clips + EntityMode->ShownClip;
    b32 Same = ((Clip->Kind == EntityMode->PreviewClip.Kind) &&
                StringsAreEqual(Clip->FileName, EntityMode->PreviewClip.FileName));
    if(!Same)
    {
        ReleaseEntityPreview(TextureOpQueue, EntityMode);
        EntityMode->PreviewClip = *Clip;
        EntityMode->Time = 0.0f;
        if(Clip->Kind != EntityLook_None)
        {
            platform_file_type Type = (Clip->Kind == EntityLook_Bitmap) ? PlatformFileType_BMP : PlatformFileType_SSBMP;
            EntityMode->Preview = LoadImageFile(Clip->FileName, Type, 0);
            if(EntityMode->Preview.Memory)
            {
                AllocateBitmap(TextureOpQueue, &EntityMode->Preview);
            }
            EntityMode->PreviewChoice = FindLookChoice(EntityMode, Clip);
        }
    }
}

internal void
RenderEntityShape(render_group *RenderGroup, object_transform *Flat, entity_shape *Shape, v2 Foot, r32 Zoom, v4 Color)
{
    v2 Center = Foot + Zoom*Shape->Center;
    if(Shape->Shape == EntityCollision_Circle)
    {
        PushCircleOutline(RenderGroup, Flat, V3(Center, 8.0f), Zoom*Shape->Size.x, 32, Color);
    }
    else if(Shape->Shape == EntityCollision_Box)
    {
        PushRectOutline(RenderGroup, Flat, RectCenterDim(Center, Zoom*Shape->Size), 8.0f, Color, 1.0f);
    }
}

internal void
RenderEntityPreview(editor_mode_entity *EntityMode, render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                    rectangle2 Rect, r32 Wheel)
{
    if(IsInRectangle(Rect, MouseP) && (Wheel != 0.0f))
    {
        EntityMode->Zoom = Clamp(1.0f, EntityMode->Zoom + ((Wheel > 0.0f) ? 1.0f : -1.0f), 16.0f);
    }

    r32 Zoom = EntityMode->Zoom;
    v2 Foot = V2(GetCenter(Rect).x, Rect.Min.y + 0.3f*GetDim(Rect).y);
    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 4.0f, V4(0.18f, 0.18f, 0.2f, 1.0f));
    PushLine(RenderGroup, Flat, V3(Rect.Min.x, Foot.y, 5.0f), V3(Rect.Max.x, Foot.y, 5.0f), V4(1, 1, 1, 0.15f));

    loaded_bitmap *Preview = &EntityMode->Preview;
    if(Preview->Memory)
    {
        if(EntityMode->PreviewClip.Kind == EntityLook_Bitmap)
        {
            Preview->AlignPercentage = (EntityMode->PreviewChoice >= 0) ?
                EntityMode->LookChoices[EntityMode->PreviewChoice].Align : V2(0.5f, 0.0f);
            PushBitmap(RenderGroup, Flat, Preview, Zoom*(r32)Preview->Height, V3(Foot, 6.0f));
        }
        else if(EntityMode->PreviewChoice >= 0)
        {
            // NOTE(pvlso): One frame of the strip, the strip drawn from its bottom left and clipped to the frame
            entity_look_choice *Choice = EntityMode->LookChoices + EntityMode->PreviewChoice;
            u32 Count = Maximum(1, Choice->SpriteCount);
            u32 Frame = (u32)Maximum(0, FloorReal32ToInt32(EntityMode->Time*EntityMode->PreviewClip.FramesPerSecond));
            Frame = EntityMode->PreviewClip.Loop ? (Frame % Count) : Minimum(Frame, Count - 1);
            v2 FrameDim = Zoom*V2((r32)Choice->SpriteWidth, (r32)Choice->SpriteHeight);
            v2 FrameMin = Foot - Hadamard(Choice->Align, FrameDim);
            Preview->AlignPercentage = V2(0, 0);
            RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, RectMinDim(FrameMin, FrameDim), 0.0f, 0);
            PushBitmap(RenderGroup, Flat, Preview, Zoom*(r32)Preview->Height,
                       V3(FrameMin - V2((r32)Frame*FrameDim.x, 0.0f), 6.0f));
            RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
        }
    }

    entity_file *Entity = &EntityMode->Entity;
    if(EntityMode->ShowHitArea)
    {
        RenderEntityShape(RenderGroup, Flat, &Entity->HitArea, Foot, Zoom, V4(0.3f, 0.6f, 1.0f, 1.0f));
    }
    if(EntityMode->ShowFootprint)
    {
        RenderEntityShape(RenderGroup, Flat, &Entity->Footprint, Foot, Zoom, V4(0.2f, 1.0f, 0.4f, 1.0f));
    }

    // NOTE(pvlso): The foot
    PushLine(RenderGroup, Flat, V3(Foot - V2(8, 0), 9.0f), V3(Foot + V2(8, 0), 9.0f), V4(1, 0.85f, 0.2f, 1));
    PushLine(RenderGroup, Flat, V3(Foot - V2(0, 8), 9.0f), V3(Foot + V2(0, 8), 9.0f), V4(1, 0.85f, 0.2f, 1));
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

// NOTE(pvlso): data/entities/<name>.txt from what was typed, false without a name
internal b32
EntityFileNameFrom(char *Typed, char *Dest, u32 DestSize)
{
    char Clean[ENTITY_NAME_LENGTH];
    b32 Result = (CleanFileName(Typed, Clean, sizeof(Clean)) != 0);
    FormatString(DestSize, Dest, "%s.txt", Clean);
    if(!Result)
    {
        LOG_ERROR("An entity needs a name");
    }

    return(Result);
}

internal void
SetEditedEntity(platform_texture_op_queue *TextureOpQueue, editor_mode_entity *EntityMode, entity_file Entity,
                char *FileName, b32 Dirty)
{
    ReleaseEntityPreview(TextureOpQueue, EntityMode);
    EntityMode->Entity = Entity;
    EntityMode->LastEntity = Entity;
    EntityMode->HasEntity = true;
    EntityMode->Dirty = Dirty;
    EntityMode->ShownClip = 0;
    FormatString(sizeof(EntityMode->FileName), EntityMode->FileName, "%s", FileName);
}

// NOTE(pvlso): A footprint's or hit area's shape and numbers
internal void
DrawEntityShapeEditor(nk_context *Nk, char *Label, entity_shape *Shape)
{
    nk_layout_row_template_begin(Nk, 28);
    nk_layout_row_template_push_static(Nk, 80);
    nk_layout_row_template_push_dynamic(Nk);
    nk_layout_row_template_end(Nk);
    nk_label(Nk, Label, NK_TEXT_LEFT);
    char *ShapeNames[] = {"none", "circle", "box"};
    if(nk_combo_begin_label(Nk, ShapeNames[Shape->Shape], nk_vec2(nk_widget_width(Nk), 120)))
    {
        nk_layout_row_dynamic(Nk, 20, 1);
        for(u32 Kind = 0; Kind < ArrayCount(ShapeNames); ++Kind)
        {
            nk_bool Selected = (Kind == (u32)Shape->Shape);
            if(nk_selectable_label(Nk, ShapeNames[Kind], NK_TEXT_LEFT, &Selected))
            {
                Shape->Shape = (entity_collision_shape)Kind;
                nk_combo_close(Nk);
            }
        }
        nk_combo_end(Nk);
    }
    if(Shape->Shape != EntityCollision_None)
    {
        nk_layout_row_dynamic(Nk, 26, 2);
        nk_property_float(Nk, "#x", -512.0f, &Shape->Center.x, 512.0f, 0.5f, 0.1f);
        nk_property_float(Nk, "#y", -512.0f, &Shape->Center.y, 512.0f, 0.5f, 0.1f);
        if(Shape->Shape == EntityCollision_Circle)
        {
            nk_property_float(Nk, "#radius", 0.5f, &Shape->Size.x, 512.0f, 0.5f, 0.1f);
        }
        else
        {
            nk_property_float(Nk, "#width", 0.5f, &Shape->Size.x, 1024.0f, 0.5f, 0.1f);
            nk_property_float(Nk, "#height", 0.5f, &Shape->Size.y, 1024.0f, 0.5f, 0.1f);
        }
    }
}

internal void
DrawEntityFieldEditor(editor_mode_entity *EntityMode, nk_context *Nk, entity_field_def *Field, entity_field_value *Value)
{
    nk_layout_row_template_begin(Nk, 26);
    nk_layout_row_template_push_static(Nk, 120);
    nk_layout_row_template_push_dynamic(Nk);
    nk_layout_row_template_end(Nk);
    nk_label(Nk, Field->Name, NK_TEXT_LEFT);
    switch(Field->Kind)
    {
        case EntityField_Int:
        {
            nk_property_int(Nk, "#", -1000000, &Value->Int, 1000000, 1, 0.2f);
        } break;

        case EntityField_Number:
        {
            nk_property_float(Nk, "#", -1000000.0f, &Value->Number, 1000000.0f, 0.1f, 0.05f);
        } break;

        case EntityField_Bool:
        {
            nk_bool On = (Value->Bool != 0);
            if(nk_checkbox_label(Nk, "", &On)) {Value->Bool = On;}
        } break;

        case EntityField_Text:
        {
            int Length = (int)StringLength(Value->Text);
            nk_edit_string(Nk, NK_EDIT_FIELD, Value->Text, &Length, ENTITY_TEXT_LENGTH - 1, nk_filter_default);
            Value->Text[Length] = 0;
        } break;

        case EntityField_Choice:
        {
            u32 Choice = ((u32)Value->Int < Field->ChoiceCount) ? (u32)Value->Int : 0;
            if(nk_combo_begin_label(Nk, Field->Choices[Choice], nk_vec2(nk_widget_width(Nk), 300)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 Index = 0; Index < Field->ChoiceCount; ++Index)
                {
                    nk_bool Selected = (Index == Choice);
                    if(nk_selectable_label(Nk, Field->Choices[Index], NK_TEXT_LEFT, &Selected))
                    {
                        Value->Int = (s32)Index;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }
        } break;

        case EntityField_Entity:
        {
            char *Name = EntityFileOfGUID(EntityMode, Value->Entity);
            char Unknown[32];
            if(Value->Entity && !Name)
            {
                char Hex[17];
                EntityGUIDToHex(Value->Entity, Hex);
                FormatString(sizeof(Unknown), Unknown, "? %s", Hex);
                Name = Unknown;
            }
            if(nk_combo_begin_label(Nk, Name ? Name : "(none)", nk_vec2(nk_widget_width(Nk), 300)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                nk_bool NoneSelected = (Value->Entity == 0);
                if(nk_selectable_label(Nk, "(none)", NK_TEXT_LEFT, &NoneSelected))
                {
                    Value->Entity = 0;
                    nk_combo_close(Nk);
                }
                for(u32 Index = 0; Index < EntityMode->EntityFileCount; ++Index)
                {
                    nk_bool Selected = (EntityMode->EntityGUIDs[Index] == Value->Entity);
                    if(EntityMode->EntityGUIDs[Index] && nk_selectable_label(Nk, EntityMode->EntityFiles[Index], NK_TEXT_LEFT, &Selected))
                    {
                        Value->Entity = EntityMode->EntityGUIDs[Index];
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }
        } break;

        InvalidDefaultCase;
    }
}

internal void
DrawEntityEditorUI(editor_mode_entity *EntityMode, ui_state *UIState)
{
    nk_context *Nk = UIState->Nk;
    entity_project *Project = EntityMode->Project;

    nk_layout_space_begin(Nk, NK_STATIC, 1000, INT_MAX);
    nk_layout_space_push(Nk, nk_rect(10, 10, 410, 1000));
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Entity Editor Panel", 0))
    {
        nk_layout_row_dynamic(Nk, 30, 2);
        if(nk_button_label(Nk, "Exit")) {EntityMode->Exit = true;}
        nk_labelf(Nk, NK_TEXT_LEFT, "Project: %s", EntityMode->ProjectName);
        if(EntityMode->ExitArmed)
        {
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Unsaved changes: Exit again to drop them.", NK_TEXT_LEFT);
        }

        // NOTE(pvlso): The definition's file: open one of data/entities, or a name for New and Save as
        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "Definitions (data/entities):", NK_TEXT_LEFT);
        nk_layout_row_template_begin(Nk, 28);
        nk_layout_row_template_push_dynamic(Nk);
        nk_layout_row_template_push_static(Nk, 100);
        nk_layout_row_template_end(Nk);
        const char *FilePreview = EntityMode->EntityFileCount ? EntityMode->EntityFiles[EntityMode->EntityFileIndex] : "<none>";
        if(nk_combo_begin_label(Nk, FilePreview, nk_vec2(nk_widget_width(Nk), 300)))
        {
            nk_layout_row_dynamic(Nk, 20, 1);
            for(u32 Index = 0; Index < EntityMode->EntityFileCount; ++Index)
            {
                nk_bool Selected = (Index == EntityMode->EntityFileIndex);
                if(nk_selectable_label(Nk, EntityMode->EntityFiles[Index], NK_TEXT_LEFT, &Selected))
                {
                    EntityMode->EntityFileIndex = Index;
                    nk_combo_close(Nk);
                }
            }
            nk_combo_end(Nk);
        }
        if(nk_button_label(Nk, "Open")) {EntityMode->OpenRequested = true;}

        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "Name (for New and Save as):", NK_TEXT_LEFT);
        nk_layout_row_template_begin(Nk, 28);
        nk_layout_row_template_push_dynamic(Nk);
        nk_layout_row_template_push_static(Nk, 100);
        nk_layout_row_template_end(Nk);
        nk_edit_string(Nk, NK_EDIT_FIELD, EntityMode->NameInput, &EntityMode->NameInputLength,
                       (int)sizeof(EntityMode->NameInput) - 1, nk_filter_default);
        EntityMode->NameInput[EntityMode->NameInputLength] = 0;
        if(nk_button_label(Nk, "New")) {EntityMode->NewRequested = true;}
        nk_layout_row_dynamic(Nk, 28, 3);
        if(EntityMode->HasEntity)
        {
            if(nk_button_label(Nk, EntityMode->Dirty ? "Save *" : "Save")) {EntityMode->SaveRequested = true;}
            if(nk_button_label(Nk, "Save as")) {EntityMode->SaveAsRequested = true;}
        }
        if(nk_button_label(Nk, "Reload lists")) {EntityMode->ReloadRequested = true;}

        if(EntityMode->HasEntity)
        {
            entity_file *Entity = &EntityMode->Entity;
            char Hex[17];
            EntityGUIDToHex(Entity->GUID, Hex);
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "", NK_TEXT_LEFT);
            nk_labelf(Nk, NK_TEXT_LEFT, "%s%s", EntityMode->FileName, EntityMode->Dirty ? "  (not saved)" : "");
            nk_labelf(Nk, NK_TEXT_LEFT, "GUID %s", Hex);

            // NOTE(pvlso): The engine's core
            nk_layout_row_template_begin(Nk, 28);
            nk_layout_row_template_push_static(Nk, 80);
            nk_layout_row_template_push_dynamic(Nk);
            nk_layout_row_template_end(Nk);
            nk_label(Nk, "Display", NK_TEXT_LEFT);
            int DisplayLength = (int)StringLength(Entity->DisplayName);
            nk_edit_string(Nk, NK_EDIT_FIELD, Entity->DisplayName, &DisplayLength, ENTITY_DISPLAY_LENGTH - 1,
                           nk_filter_default);
            Entity->DisplayName[DisplayLength] = 0;

            nk_label(Nk, "Category", NK_TEXT_LEFT);
            const char *CategoryPreview = (Entity->Category && (Entity->Category <= Project->CategoryCount)) ?
                Project->Categories[Entity->Category - 1] : "(none)";
            if(nk_combo_begin_label(Nk, CategoryPreview, nk_vec2(nk_widget_width(Nk), 300)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 Category = 0; Category <= Project->CategoryCount; ++Category)
                {
                    nk_bool Selected = (Category == Entity->Category);
                    char *Label = Category ? Project->Categories[Category - 1] : (char *)"(none)";
                    if(nk_selectable_label(Nk, Label, NK_TEXT_LEFT, &Selected))
                    {
                        Entity->Category = Category;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }

            // NOTE(pvlso): A look per clip; the picked clip is the one shown
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Clips (click one to show it):", NK_TEXT_LEFT);
            if(!EntityMode->LookChoiceCount)
            {
                nk_label(Nk, "No bitmaps or sprite sheets stored yet", NK_TEXT_LEFT);
            }
            for(u32 ClipIndex = 0; ClipIndex < Project->ClipCount; ++ClipIndex)
            {
                entity_clip *Clip = Entity->Clips + ClipIndex;
                nk_layout_row_template_begin(Nk, 26);
                nk_layout_row_template_push_static(Nk, 90);
                nk_layout_row_template_push_dynamic(Nk);
                nk_layout_row_template_end(Nk);
                nk_bool Shown = (ClipIndex == EntityMode->ShownClip);
                if(nk_selectable_label(Nk, Project->Clips[ClipIndex], NK_TEXT_LEFT, &Shown))
                {
                    EntityMode->ShownClip = ClipIndex;
                    EntityMode->Time = 0.0f;
                }
                s32 Choice = (Clip->Kind != EntityLook_None) ? FindLookChoice(EntityMode, Clip) : -1;
                const char *LookPreview = (Clip->Kind == EntityLook_None) ? "(none)" :
                    ((Choice >= 0) ? EntityMode->LookLabels[Choice] : Clip->FileName);
                if(nk_combo_begin_label(Nk, LookPreview, nk_vec2(nk_widget_width(Nk), 300)))
                {
                    nk_layout_row_dynamic(Nk, 20, 1);
                    nk_bool NoneSelected = (Clip->Kind == EntityLook_None);
                    if(nk_selectable_label(Nk, "(none)", NK_TEXT_LEFT, &NoneSelected))
                    {
                        *Clip = {};
                        nk_combo_close(Nk);
                    }
                    for(u32 ChoiceIndex = 0; ChoiceIndex < EntityMode->LookChoiceCount; ++ChoiceIndex)
                    {
                        nk_bool Selected = ((s32)ChoiceIndex == Choice);
                        if(nk_selectable_label(Nk, EntityMode->LookLabels[ChoiceIndex], NK_TEXT_LEFT, &Selected))
                        {
                            entity_look_choice *Picked = EntityMode->LookChoices + ChoiceIndex;
                            Clip->Kind = Picked->Kind;
                            FormatString(ENTITY_LOOK_NAME_LENGTH, Clip->FileName, "%s", Picked->FileName);
                            if(Clip->FramesPerSecond <= 0.0f) {Clip->FramesPerSecond = 8.0f; Clip->Loop = true;}
                            EntityMode->ShownClip = ClipIndex;
                            nk_combo_close(Nk);
                        }
                    }
                    nk_combo_end(Nk);
                }
                if(Clip->Kind == EntityLook_SpriteSheet)
                {
                    nk_layout_row_template_begin(Nk, 26);
                    nk_layout_row_template_push_static(Nk, 90);
                    nk_layout_row_template_push_dynamic(Nk);
                    nk_layout_row_template_push_static(Nk, 70);
                    nk_layout_row_template_end(Nk);
                    nk_spacer(Nk);
                    nk_property_float(Nk, "#fps", 0.5f, &Clip->FramesPerSecond, 60.0f, 0.5f, 0.1f);
                    nk_bool Loop = (Clip->Loop != 0);
                    if(nk_checkbox_label(Nk, "loop", &Loop)) {Clip->Loop = Loop; EntityMode->Time = 0.0f;}
                }
            }

            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Footprint blocks movement (green):", NK_TEXT_LEFT);
            DrawEntityShapeEditor(Nk, "Footprint", &Entity->Footprint);
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Hit area takes spells and interaction (blue):", NK_TEXT_LEFT);
            DrawEntityShapeEditor(Nk, "Hit area", &Entity->HitArea);

            // NOTE(pvlso): The project's blocks: ticked ones are used, their fields below them
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "", NK_TEXT_LEFT);
            nk_label(Nk, "Blocks (data/txts/entity_blocks.txt):", NK_TEXT_LEFT);
            if(!Project->BlockCount)
            {
                nk_label(Nk, "(none declared)", NK_TEXT_LEFT);
            }
            for(u32 BlockIndex = 0; BlockIndex < Project->BlockCount; ++BlockIndex)
            {
                entity_block_def *Block = Project->Blocks + BlockIndex;
                nk_layout_row_dynamic(Nk, 26, 1);
                nk_bool Uses = ((Entity->Blocks & (1u << BlockIndex)) != 0);
                if(nk_checkbox_label(Nk, Block->Name, &Uses))
                {
                    Entity->Blocks ^= (1u << BlockIndex);
                    if(!Uses) {ResetEntityBlock(Entity, Project, BlockIndex);}
                }
                if(Uses)
                {
                    for(u32 FieldIndex = 0; FieldIndex < Block->FieldCount; ++FieldIndex)
                    {
                        DrawEntityFieldEditor(EntityMode, Nk, Block->Fields + FieldIndex,
                                              &Entity->Fields[BlockIndex][FieldIndex]);
                    }
                }
            }
        }

        nk_layout_row_dynamic(Nk, 18, 1);
        nk_label(Nk, "", NK_TEXT_LEFT);
        nk_label(Nk, "Clips: the bitmaps and sprite sheets stored", NK_TEXT_LEFT);
        nk_label(Nk, "in the assets mode. Preview: the wheel zooms,", NK_TEXT_LEFT);
        nk_label(Nk, "yellow is the foot.", NK_TEXT_LEFT);

        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);
}

internal b32
UpdateAndRenderEntityEditor(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                            platform_texture_op_queue *TextureOpQueue)
{
    TIMED_FUNCTION();

    editor_mode_entity *EntityMode = EditorState->EntityMode;
    ui_state *UIState = &EditorState->UIState;
    render_group *RenderGroup = &UIState->RenderGroup;
    object_transform Flat = DefaultFlatTransform();
    v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;

    if(EntityMode->Exit && EntityMode->Dirty && !EntityMode->ExitArmed)
    {
        EntityMode->Exit = false;
        EntityMode->ExitArmed = true;
    }
    if(EntityMode->Exit)
    {
        ReleaseEntityPreview(TextureOpQueue, EntityMode);
        PlayTitleScreen(EditorState, TranState);
        return(true);
    }

    if(EntityMode->ReloadRequested)
    {
        // NOTE(pvlso): The blocks file, the stored assets or data/entities changed. What's being made keeps its values
        // by place: a block file whose blocks moved should be followed by opening the definition again.
        EntityMode->ReloadRequested = false;
        EntityMode->Project = ReadEntityProject(EntityMode->Arena);
        ListLookChoices(EditorState, EntityMode);
        ListEntityFiles(EntityMode);
        EntityMode->PreviewClip = {};
        EntityMode->ShownClip = Minimum(EntityMode->ShownClip, EntityMode->Project->ClipCount - 1);
    }
    entity_project *Project = EntityMode->Project;
    if(EntityMode->NewRequested)
    {
        EntityMode->NewRequested = false;
        char FileName[64];
        if(EntityFileNameFrom(EntityMode->NameInput, FileName, sizeof(FileName)))
        {
            if(Platform.FileExists(FileName, PlatformFileType_Entity))
            {
                LOG_ERROR("Entity %s already exists: Open it, or pick another name", FileName);
            }
            else
            {
                SetEditedEntity(TextureOpQueue, EntityMode, NewEntityDefinition(Project), FileName, true);
            }
        }
    }
    if(EntityMode->OpenRequested)
    {
        EntityMode->OpenRequested = false;
        if(EntityMode->EntityFileCount)
        {
            char *FileName = EntityMode->EntityFiles[EntityMode->EntityFileIndex];
            entity_file Entity = ReadEntityFile(FileName, Project, &TranState->TranArena);
            if(Entity.Loaded)
            {
                b32 NeedsGUID = (Entity.GUID == 0);
                if(NeedsGUID)
                {
                    Entity.GUID = NewEntityGUID();
                    LOG_WARNING("Entity %s had no GUID, it has one now: save it", FileName);
                }
                SetEditedEntity(TextureOpQueue, EntityMode, Entity, FileName, NeedsGUID);
                LOG("Opened the entity %s%s", FileName, Entity.ErrorCount ? " (with errors, see above)" : "");
            }
        }
    }
    if((EntityMode->SaveRequested || EntityMode->SaveAsRequested) && EntityMode->HasEntity)
    {
        char FileName[64];
        FormatString(sizeof(FileName), FileName, "%s", EntityMode->FileName);
        b32 HaveName = EntityMode->SaveRequested || EntityFileNameFrom(EntityMode->NameInput, FileName, sizeof(FileName));
        if(HaveName && EntityMode->SaveAsRequested && Platform.FileExists(FileName, PlatformFileType_Entity))
        {
            LOG_ERROR("Entity %s already exists, pick another name", FileName);
            HaveName = false;
        }
        if(HaveName)
        {
            // NOTE(pvlso): Save as makes another definition: it gets a GUID of its own
            entity_file *Entity = &EntityMode->Entity;
            u64 OldGUID = Entity->GUID;
            if(EntityMode->SaveAsRequested) {Entity->GUID = NewEntityGUID();}
            if(WriteEntityFile(FileName, Entity, Project, &TranState->TranArena))
            {
                FormatString(sizeof(EntityMode->FileName), EntityMode->FileName, "%s", FileName);
                EntityMode->Dirty = false;
                EntityMode->LastEntity = *Entity;
                ListEntityFiles(EntityMode);
                LOG("Saved the entity %s", FileName);
            }
            else
            {
                Entity->GUID = OldGUID;
            }
        }
        EntityMode->SaveRequested = EntityMode->SaveAsRequested = false;
    }

    // NOTE(pvlso): The canvas right of the panel: the shown clip's look
    nk_context *Nk = UIState->Nk;
    rectangle2 PreviewRect = RectMinMax(V2(-530.0f, -530.0f), V2(950.0f, 470.0f));
    if(EntityMode->HasEntity)
    {
        if(!StructuresAreEqual(EntityMode->Entity, EntityMode->LastEntity))
        {
            EntityMode->Dirty = true;
            EntityMode->LastEntity = EntityMode->Entity;
        }
        UpdateEntityPreview(TextureOpQueue, EntityMode);
        RenderEntityPreview(EntityMode, RenderGroup, &Flat, MouseP, PreviewRect, Nk->input.mouse.scroll_delta.y);
    }
    else
    {
        PushRect(RenderGroup, &Flat, PreviewRect, 4.0f, V4(0.12f, 0.12f, 0.14f, 1.0f));
    }
    EntityMode->Time += Input->dtForFrame;

    DrawEntityEditorUI(EntityMode, UIState);

    return(false);
}
