/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline b32
UIIsHex(char Char)
{
    b32 Result = (((Char >= '0') && (Char <= '9')) ||
                  ((Char >= 'A') && (Char <= 'F')));

    return(Result);
}

inline u32
UIGetHex(char Char)
{
    u32 Result = 0;

    if((Char >= '0') && (Char <= '9'))
    {
        Result = Char - '0';
    }
    else if((Char >= 'A') && (Char <= 'F'))
    {
        Result = 0xA + (Char - 'A');
    }

    return(Result);
}

internal void
ResetCursorArray(array_cursor *Cursor)
{
    for(u32 I = 0;
        I < Cursor->ArrayCount;
        ++I)
    {
        Cursor->Array[I] = I;
    }
}

internal void
InitializeCursor(array_cursor *Cursor, u32 CursorArrayCount)
{
    Cursor->ArrayPosition = 0;
    Cursor->ArrayCount = CursorArrayCount;
    ResetCursorArray(Cursor);
}

internal void
ReInitializeCursor(array_cursor *Cursor, u32 CursorArrayCount)
{
    Cursor->ArrayPosition = 0;
    Cursor->ArrayCount = CursorArrayCount;
    ResetCursorArray(Cursor);
}

internal void
ChangeCursorPositionForScrollWindow(array_cursor *Cursor, u32 SourceArrayCount, s16 MouseZ)
{
    if(MouseZ != 0)
    {
        u32 CursorLastIndex = Cursor->ArrayCount - 1;
        s32 Count = MouseZ > 0 ? MouseZ : MouseZ * -1;
        for(s32 MouseRotIndex = 0;
            MouseRotIndex < Count;
            ++MouseRotIndex)
        {
            if(MouseZ > 0)
            {
                s32 NewFirst = Cursor->Array[0] - 1;
                for(u32 I = CursorLastIndex; I > 0; --I)
                    Cursor->Array[I] = Cursor->Array[I - 1];

                if(NewFirst == -1)
                    NewFirst = SourceArrayCount - 1;

                Cursor->Array[0] = NewFirst;
            }
            else
            {
                u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                for(u32 I = 0; I < Cursor->ArrayCount; ++I)
                    Cursor->Array[I] = Cursor->Array[I + 1];

                if(NewLast == SourceArrayCount)
                    NewLast = 0;

                Cursor->Array[CursorLastIndex] = NewLast;
            }
        }
    }
}

internal void
ChangeCursorPositionForToolBar(array_cursor *Cursor, u32 SourceArrayCount, s16 MouseZ)
{
    if(MouseZ != 0)
    {
        u32 CursorLastIndex = Cursor->ArrayCount - 1;
        s32 Count = MouseZ > 0 ? MouseZ : MouseZ * -1;
        for(s32 MouseRotIndex = 0;
            MouseRotIndex < Count;
            ++MouseRotIndex)
        {
            if(MouseZ > 0)
            {
                if(Cursor->ArrayPosition == 0)
                {
                    s32 NewFirst = Cursor->Array[0] - 1;
                    for(u32 I = CursorLastIndex; I > 0; --I)
                        Cursor->Array[I] = Cursor->Array[I - 1];

                    if(NewFirst == -1)
                        NewFirst = SourceArrayCount - 1;

                    Cursor->Array[0] = NewFirst;
                }
                else
                    --Cursor->ArrayPosition;
            }
            else
            {
                if(Cursor->ArrayPosition == CursorLastIndex)
                {
                    u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                    for(u32 I = 0; I < Cursor->ArrayCount; ++I)
                        Cursor->Array[I] = Cursor->Array[I + 1];

                    if(NewLast == SourceArrayCount)
                        NewLast = 0;

                    Cursor->Array[CursorLastIndex] = NewLast;
                }
                else
                    ++Cursor->ArrayPosition;
            }
        }
    }
}

internal void
UIDrawWindowOutline(ui_state *UIState, v2 Center, v2 Dim, b32 StandardLable = true)
{
    render_group *RenderGroup = &UIState->RenderGroup;
    UIState->BackingTransform.OffsetP += V3(Center, 0.0f);
    v2 HalfDim = 0.5f*Dim;
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 0), Dim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 2.0f), Dim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 3.0f), Dim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 4.0f), Dim - V2(16.0f, 16.0f),
             UI_COLOR_RGBA1_67412CFF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 5.0f), Dim - V2(24.0f, 24.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 6.0f), Dim - V2(28.0f, 28.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 7.0f), Dim - V2(32.0f, 32.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 8.0f), Dim - V2(36.0f, 36.0f),
             UI_COLOR_RGBA1_67412CFF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 9.0f), Dim - V2(44.0f, 44.0f),
             UI_COLOR_RGBA1_4D3020FF);

    if(StandardLable)
    {
        r32 YOffset = HalfDim.y - 42.0f;
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 10.0f), V2(Dim.x - 44.0f, 48.0f),
                 UI_COLOR_RGBA1_67412CFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 11.0f), V2(Dim.x - 44.0f, 40.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 12.0f), V2(Dim.x - 48.0f, 36.0f),
                 UI_COLOR_RGBA1_B97A57FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 13.0f), V2(Dim.x - 52.0f, 32.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 14.0f), V2(Dim.x - 56.0f, 28.0f),
                 UI_COLOR_RGBA1_CFEEF5FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(0.5f, HalfDim.y - 43.0f, 15.0f), V2(Dim.x - 58.0f, 26.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0.0f, YOffset, 16.0f), V2(Dim.x - 60.0f, 24.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
    }
    
    r32 Ax[2] = {HalfDim.x - 12.0f, -(HalfDim.x - 12.0f)};
    r32 Ay[2] = {HalfDim.y - 3.0f, -(HalfDim.y - 3.0f)};

    r32 Bx[2] = {HalfDim.x - 3.0f, -(HalfDim.x - 3.0f)};
    r32 By[2] = {HalfDim.y - 12.0f, -(HalfDim.y - 12.0f)};

    r32 Cx[2] = {HalfDim.x - 10.0f, -(HalfDim.x - 10.0f)};
    r32 Cy[2] = {HalfDim.y - 10.0f, -(HalfDim.y - 10.0f)};

    r32 Dx[2] = {HalfDim.x - 8.0f, -(HalfDim.x - 8.0f)};
    r32 Dy[2] = {HalfDim.y - 8.0f, -(HalfDim.y - 8.0f)};

    r32 Ex[2] = {HalfDim.x - 1.0f, -(HalfDim.x - 1.0f)};
    r32 Ey[2] = {HalfDim.y - 1.0f, -(HalfDim.y - 1.0f)};

    r32 Fx[2] = {HalfDim.x - 5.0f, -(HalfDim.x - 5.0f)};
    r32 Fy[2] = {HalfDim.y - 6.0f, -(HalfDim.y - 6.0f)};

    r32 Gx[2] = {HalfDim.x - 11.0f, -(HalfDim.x - 11.0f)};
    r32 Gy[2] = {HalfDim.y - 7.0f, -(HalfDim.y - 7.0f)};

    r32 Hx[2] = {HalfDim.x - 13.0f, -(HalfDim.x - 13.0f)};

    u32 Indecies[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};

    for(u32 Itter = 0;
        Itter < 4;
        ++Itter)
    {
        u32 IndexX = Indecies[Itter][0];
        u32 IndexY = Indecies[Itter][1];

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ax[IndexX], Ay[IndexY], 10.0f), V2(32.0f, 14.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], By[IndexY], 10.0f), V2(14.0f, 32.0f),
                 UI_COLOR_RGBA1_4D3020FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ax[IndexX], Ay[IndexY], 11.0f), V2(28.0f, 10.0f),
                 UI_COLOR_RGBA1_B97A57FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], By[IndexY], 11.0f), V2(10.0f, 28.0f),
                 UI_COLOR_RGBA1_B97A57FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Cx[IndexX], Cy[IndexY], 12.0f), V2(12.0f, 12.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Dy[IndexY], 12.0f), V2(12.0f, 12.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Ey[IndexY], 12.0f), V2(8.0f, 2.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ex[IndexX], Dy[IndexY], 12.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_3F48CCFF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], Dy[IndexY], 13.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Fx[IndexX], Dy[IndexY], 13.0f), V2(2.0f, 12.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Fy[IndexY], 13.0f), V2(4.0f, 8.0f),
                 UI_COLOR_RGBA1_99D9EAFF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Gx[IndexX], Fy[IndexY], 14.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Hx[IndexX], Gy[IndexY], 13.0f), V2(2.0f, 6.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Cx[IndexX], By[IndexY], 13.0f), V2(8.0f, 4.0f),
                 UI_COLOR_RGBA1_00A2E8FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Dy[IndexY], 14.0f), V2(4.0f, 4.0f),
                 UI_COLOR_RGBA1_FFFFFFFF);
    }
}

internal void
BeginUI(ui_state *UIState, editor_render_commands *Commands, engine_assets *Assets, u32 MainGenerationID,
        u32 Width, u32 Height, nk_context *Nk, v2 UIScale)
{
    if(!UIState->Initialized)
    {
        UIState->Nk = Nk;
        
        UIState->Initialized = true;
    }

    UIState->RenderGroup = BeginRenderGroup(Assets, Commands, MainGenerationID, false, Width, Height);

    UIState->GlobalWidth = (r32)Width;
    UIState->GlobalHeight = (r32)Height;

    f32 Ratio = (f32)UI_BASE_RESOLUTION_X/(f32)UI_BASE_RESOLUTION_Y;
    Orthographic(&UIState->RenderGroup, UIScale.x);
    Clear(&UIState->RenderGroup, UI_COLOR_RGBA1_4D3020FF);

    UIState->LeftEdge = -0.5f*Width;
    UIState->RightEdge = 0.5f*Width;

    UIState->TextTransform = DefaultFlatTransform();
    UIState->ShadowTransform = DefaultFlatTransform();
    UIState->UITransform = DefaultFlatTransform();
    UIState->BackingTransform = DefaultFlatTransform();

    UIState->BackingTransform.ChunkZ = 100000;
    UIState->ShadowTransform.ChunkZ = 200000;
    UIState->UITransform.ChunkZ = 300000;
    UIState->TextTransform.ChunkZ = 400000;

    UIState->DefaultClipRect = UIState->RenderGroup.CurrentClipRectIndex;
}

internal void
EndUI(editor_state *EditorState, ui_state *UIState, engine_input *Input)
{
    TIMED_FUNCTION();

    render_group *RenderGroup = &UIState->RenderGroup;

    UIState->AltUI = Input->MouseButtons[PlatformMouseButton_Right].EndedDown;
    UIState->MouseZ = Input->MouseZ;
    object_transform Flat = DefaultFlatTransform();
    v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
    
    EndRenderGroup(&UIState->RenderGroup);
}
