/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): The entity editor: see editor_entity_mode.h

internal void
ListEntityFiles(editor_mode_entity *EntityMode)
{
    EntityMode->EntityFileCount = Platform.ListFilesInDirectory(PlatformFileType_Entity, 0, 0);
    EntityMode->EntityFiles = PushArray(EntityMode->Arena, Maximum(1, EntityMode->EntityFileCount), char *);
    Platform.ListFilesInDirectory(PlatformFileType_Entity, EntityMode->EntityFiles, EntityMode->Arena);
    EntityMode->EntityFileIndex = 0;
}

// NOTE(pvlso): The project's stored bitmaps and sprite sheets, what an entity can look like
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
        FormatString(ENTITY_LOOK_NAME_LENGTH, Choice->Look.FileName, "%s", Asset->SourceFileName);
        if(Asset->Type == KESA_Bitmap)
        {
            Choice->Look.Kind = EntityLook_Bitmap;
            Choice->Align = Asset->Bitmap.AlignPercentage;
        }
        else
        {
            Choice->Look.Kind = EntityLook_SpriteSheet;
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
FindLookChoice(editor_mode_entity *EntityMode, entity_look *Look)
{
    s32 Result = -1;
    for(u32 ChoiceIndex = 0; ChoiceIndex < EntityMode->LookChoiceCount; ++ChoiceIndex)
    {
        entity_look *Choice = &EntityMode->LookChoices[ChoiceIndex].Look;
        if((Choice->Kind == Look->Kind) && StringsAreEqual(Choice->FileName, Look->FileName)) {Result = (s32)ChoiceIndex;}
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
    EntityMode->Types = ReadEntityTypes(EntityMode->Arena);
    ListLookChoices(EditorState, EntityMode);
    ListEntityFiles(EntityMode);

    EditorState->EntityMode = EntityMode;
}

internal void
ReleaseEntityPreview(platform_texture_op_queue *TextureOpQueue, editor_mode_entity *EntityMode)
{
    DeallocateBitmap(TextureOpQueue, &EntityMode->Preview);
    EntityMode->Preview = {};
    EntityMode->PreviewLook = {};
    EntityMode->PreviewChoice = -1;
}

// NOTE(pvlso): The shown state's look loaded, when it isn't the one loaded already
internal void
UpdateEntityPreview(platform_texture_op_queue *TextureOpQueue, editor_mode_entity *EntityMode)
{
    entity_look *Look = EntityMode->Entity.Looks + EntityMode->ShownState;
    b32 Same = ((Look->Kind == EntityMode->PreviewLook.Kind) &&
                StringsAreEqual(Look->FileName, EntityMode->PreviewLook.FileName));
    if(!Same)
    {
        ReleaseEntityPreview(TextureOpQueue, EntityMode);
        EntityMode->PreviewLook = *Look;
        if(Look->Kind != EntityLook_None)
        {
            platform_file_type Type = (Look->Kind == EntityLook_Bitmap) ? PlatformFileType_BMP : PlatformFileType_SSBMP;
            EntityMode->Preview = LoadImageFile(Look->FileName, Type, 0);
            if(EntityMode->Preview.Memory)
            {
                AllocateBitmap(TextureOpQueue, &EntityMode->Preview);
            }
            EntityMode->PreviewChoice = FindLookChoice(EntityMode, Look);
        }
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
        if(EntityMode->PreviewLook.Kind == EntityLook_Bitmap)
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
            u32 Frame = (u32)(EntityMode->Time*8.0f) % Count;
            v2 FrameDim = Zoom*V2((r32)Choice->SpriteWidth, (r32)Choice->SpriteHeight);
            v2 FrameMin = Foot - Hadamard(Choice->Align, FrameDim);
            Preview->AlignPercentage = V2(0, 0);
            u32 FrameClip = PushClipRect(RenderGroup, Flat, RectMinDim(FrameMin, FrameDim), 0.0f, 0);
            RenderGroup->CurrentClipRectIndex = FrameClip;
            PushBitmap(RenderGroup, Flat, Preview, Zoom*(r32)Preview->Height,
                       V3(FrameMin - V2((r32)Frame*FrameDim.x, 0.0f), 6.0f));
            RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
        }
    }

    entity_file *Entity = &EntityMode->Entity;
    v4 CollisionColor = V4(0.2f, 1.0f, 0.4f, 1.0f);
    v2 Center = Foot + Zoom*Entity->CollisionCenter;
    if(Entity->Collision == EntityCollision_Circle)
    {
        PushCircleOutline(RenderGroup, Flat, V3(Center, 8.0f), Zoom*Entity->CollisionSize.x, 32, CollisionColor);
    }
    else if(Entity->Collision == EntityCollision_Box)
    {
        PushRectOutline(RenderGroup, Flat, RectCenterDim(Center, Zoom*Entity->CollisionSize), 8.0f, CollisionColor, 1.0f);
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
    char Clean[48];
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
    EntityMode->ShownState = Entity.State;
    FormatString(sizeof(EntityMode->FileName), EntityMode->FileName, "%s", FileName);
}

internal void
DrawEntityEditorUI(editor_mode_entity *EntityMode, ui_state *UIState)
{
    nk_context *Nk = UIState->Nk;
    entity_types *Types = EntityMode->Types;

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

        // NOTE(pvlso): The entity's file: open one of data/entities, or a name and a type for New
        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "Entities (data/entities):", NK_TEXT_LEFT);
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
        nk_layout_row_dynamic(Nk, 28, 1);
        nk_edit_string(Nk, NK_EDIT_FIELD, EntityMode->NameInput, &EntityMode->NameInputLength,
                       (int)sizeof(EntityMode->NameInput) - 1, nk_filter_default);
        EntityMode->NameInput[EntityMode->NameInputLength] = 0;
        if(Types->Count)
        {
            nk_layout_row_template_begin(Nk, 28);
            nk_layout_row_template_push_dynamic(Nk);
            nk_layout_row_template_push_static(Nk, 100);
            nk_layout_row_template_end(Nk);
            if(nk_combo_begin_label(Nk, Types->Types[EntityMode->NewType].Name, nk_vec2(nk_widget_width(Nk), 300)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 TypeIndex = 0; TypeIndex < Types->Count; ++TypeIndex)
                {
                    nk_bool Selected = (TypeIndex == EntityMode->NewType);
                    if(nk_selectable_label(Nk, Types->Types[TypeIndex].Name, NK_TEXT_LEFT, &Selected))
                    {
                        EntityMode->NewType = TypeIndex;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }
            if(nk_button_label(Nk, "New")) {EntityMode->NewRequested = true;}
        }
        else
        {
            nk_label(Nk, "No types: data/txts/entity_types.txt", NK_TEXT_LEFT);
        }
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
            entity_type *Type = Types->Types + Entity->Type;

            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "", NK_TEXT_LEFT);
            nk_labelf(Nk, NK_TEXT_LEFT, "%s, a %s%s", EntityMode->FileName, Type->Name,
                      EntityMode->Dirty ? "  (not saved)" : "");

            // NOTE(pvlso): What every entity has
            nk_layout_row_template_begin(Nk, 28);
            nk_layout_row_template_push_static(Nk, 80);
            nk_layout_row_template_push_dynamic(Nk);
            nk_layout_row_template_end(Nk);
            nk_label(Nk, "Starts in", NK_TEXT_LEFT);
            if(nk_combo_begin_label(Nk, Type->States[Entity->State], nk_vec2(nk_widget_width(Nk), 300)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 StateIndex = 0; StateIndex < Type->StateCount; ++StateIndex)
                {
                    nk_bool Selected = (StateIndex == Entity->State);
                    if(nk_selectable_label(Nk, Type->States[StateIndex], NK_TEXT_LEFT, &Selected))
                    {
                        Entity->State = StateIndex;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }

            nk_layout_row_dynamic(Nk, 28, 1);
            nk_bool Flip = (Entity->Flip != 0);
            if(nk_checkbox_label(Nk, "Flip (drawn mirrored)", &Flip)) {Entity->Flip = Flip;}

            nk_layout_row_template_begin(Nk, 28);
            nk_layout_row_template_push_static(Nk, 80);
            nk_layout_row_template_push_dynamic(Nk);
            nk_layout_row_template_end(Nk);
            nk_label(Nk, "Collision", NK_TEXT_LEFT);
            char *ShapeNames[] = {"none", "circle", "box"};
            if(nk_combo_begin_label(Nk, ShapeNames[Entity->Collision], nk_vec2(nk_widget_width(Nk), 120)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 Shape = 0; Shape < ArrayCount(ShapeNames); ++Shape)
                {
                    nk_bool Selected = (Shape == (u32)Entity->Collision);
                    if(nk_selectable_label(Nk, ShapeNames[Shape], NK_TEXT_LEFT, &Selected))
                    {
                        Entity->Collision = (entity_collision_shape)Shape;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }
            if(Entity->Collision != EntityCollision_None)
            {
                nk_layout_row_dynamic(Nk, 26, 2);
                nk_property_float(Nk, "#x", -512.0f, &Entity->CollisionCenter.x, 512.0f, 0.5f, 0.1f);
                nk_property_float(Nk, "#y", -512.0f, &Entity->CollisionCenter.y, 512.0f, 0.5f, 0.1f);
                if(Entity->Collision == EntityCollision_Circle)
                {
                    nk_property_float(Nk, "#radius", 0.5f, &Entity->CollisionSize.x, 512.0f, 0.5f, 0.1f);
                }
                else
                {
                    nk_property_float(Nk, "#width", 0.5f, &Entity->CollisionSize.x, 1024.0f, 0.5f, 0.1f);
                    nk_property_float(Nk, "#height", 0.5f, &Entity->CollisionSize.y, 1024.0f, 0.5f, 0.1f);
                }
            }

            // NOTE(pvlso): A look per state; the picked state is the one shown
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Looks (click a state to show it):", NK_TEXT_LEFT);
            if(!EntityMode->LookChoiceCount)
            {
                nk_label(Nk, "No bitmaps or sprite sheets stored yet", NK_TEXT_LEFT);
            }
            for(u32 StateIndex = 0; StateIndex < Type->StateCount; ++StateIndex)
            {
                entity_look *Look = Entity->Looks + StateIndex;
                nk_layout_row_template_begin(Nk, 26);
                nk_layout_row_template_push_static(Nk, 90);
                nk_layout_row_template_push_dynamic(Nk);
                nk_layout_row_template_end(Nk);
                nk_bool Shown = (StateIndex == EntityMode->ShownState);
                if(nk_selectable_label(Nk, Type->States[StateIndex], NK_TEXT_LEFT, &Shown))
                {
                    EntityMode->ShownState = StateIndex;
                }
                s32 Choice = (Look->Kind != EntityLook_None) ? FindLookChoice(EntityMode, Look) : -1;
                const char *LookPreview = (Look->Kind == EntityLook_None) ? "(none)" :
                    ((Choice >= 0) ? EntityMode->LookLabels[Choice] : Look->FileName);
                if(nk_combo_begin_label(Nk, LookPreview, nk_vec2(nk_widget_width(Nk), 300)))
                {
                    nk_layout_row_dynamic(Nk, 20, 1);
                    nk_bool NoneSelected = (Look->Kind == EntityLook_None);
                    if(nk_selectable_label(Nk, "(none)", NK_TEXT_LEFT, &NoneSelected))
                    {
                        *Look = {};
                        nk_combo_close(Nk);
                    }
                    for(u32 ChoiceIndex = 0; ChoiceIndex < EntityMode->LookChoiceCount; ++ChoiceIndex)
                    {
                        nk_bool Selected = ((s32)ChoiceIndex == Choice);
                        if(nk_selectable_label(Nk, EntityMode->LookLabels[ChoiceIndex], NK_TEXT_LEFT, &Selected))
                        {
                            *Look = EntityMode->LookChoices[ChoiceIndex].Look;
                            EntityMode->ShownState = StateIndex;
                            nk_combo_close(Nk);
                        }
                    }
                    nk_combo_end(Nk);
                }
            }

            // NOTE(pvlso): The type's own fields
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_labelf(Nk, NK_TEXT_LEFT, "%s's fields:", Type->Name);
            if(!Type->FieldCount)
            {
                nk_label(Nk, "(none)", NK_TEXT_LEFT);
            }
            for(u32 FieldIndex = 0; FieldIndex < Type->FieldCount; ++FieldIndex)
            {
                entity_type_field *Field = Type->Fields + FieldIndex;
                entity_field_value *Value = Entity->Fields + FieldIndex;
                nk_layout_row_template_begin(Nk, 26);
                nk_layout_row_template_push_static(Nk, 110);
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

                    InvalidDefaultCase;
                }
            }
        }

        nk_layout_row_dynamic(Nk, 18, 1);
        nk_label(Nk, "", NK_TEXT_LEFT);
        nk_label(Nk, "Types: data/txts/entity_types.txt. Looks: the", NK_TEXT_LEFT);
        nk_label(Nk, "bitmaps and sprite sheets in the stored assets.", NK_TEXT_LEFT);
        nk_label(Nk, "Preview: the wheel zooms, yellow is the foot.", NK_TEXT_LEFT);

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
    entity_types *Types = EntityMode->Types;

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
        // NOTE(pvlso): The types file or the stored assets changed: what's being made is kept if its type still is
        EntityMode->ReloadRequested = false;
        EntityMode->Types = Types = ReadEntityTypes(EntityMode->Arena);
        ListLookChoices(EditorState, EntityMode);
        ListEntityFiles(EntityMode);
        EntityMode->NewType = 0;
        EntityMode->PreviewLook = {};
        if(EntityMode->HasEntity)
        {
            s32 TypeIndex = FindEntityType(Types, EntityMode->Entity.TypeName);
            if((TypeIndex < 0) || (Types->Types[TypeIndex].StateCount <= EntityMode->Entity.State))
            {
                LOG_ERROR("Entity %s: its type %s changed, open it again", EntityMode->FileName,
                          EntityMode->Entity.TypeName);
                ReleaseEntityPreview(TextureOpQueue, EntityMode);
                EntityMode->HasEntity = false;
            }
            else
            {
                EntityMode->Entity.Type = TypeIndex;
                EntityMode->ShownState = Minimum(EntityMode->ShownState, Types->Types[TypeIndex].StateCount - 1);
            }
        }
    }
    if(EntityMode->NewRequested)
    {
        EntityMode->NewRequested = false;
        char FileName[64];
        if(EntityFileNameFrom(EntityMode->NameInput, FileName, sizeof(FileName)) && (EntityMode->NewType < Types->Count))
        {
            if(Platform.FileExists(FileName, PlatformFileType_Entity))
            {
                LOG_ERROR("Entity %s already exists: Open it, or pick another name", FileName);
            }
            else
            {
                SetEditedEntity(TextureOpQueue, EntityMode, NewEntityOfType(Types, EntityMode->NewType), FileName, true);
            }
        }
    }
    if(EntityMode->OpenRequested)
    {
        EntityMode->OpenRequested = false;
        if(EntityMode->EntityFileCount)
        {
            char *FileName = EntityMode->EntityFiles[EntityMode->EntityFileIndex];
            entity_file Entity = ReadEntityFile(FileName, Types, &TranState->TranArena);
            if(Entity.Loaded)
            {
                SetEditedEntity(TextureOpQueue, EntityMode, Entity, FileName, false);
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
        if(HaveName && WriteEntityFile(FileName, &EntityMode->Entity, Types, &TranState->TranArena))
        {
            FormatString(sizeof(EntityMode->FileName), EntityMode->FileName, "%s", FileName);
            EntityMode->Dirty = false;
            ListEntityFiles(EntityMode);
            LOG("Saved the entity %s", FileName);
        }
        EntityMode->SaveRequested = EntityMode->SaveAsRequested = false;
    }

    // NOTE(pvlso): The canvas right of the panel: the shown state's look
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
