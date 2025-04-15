/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

internal void
PlayTitleScreen(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_TitleScreen);

    GameState->FadeState = FadeState_FadeOut;
    if(GameState->MusicState != MusicState_Ambient)
    {
        ChangeBackgroundMusic(GameState, MusicState_Ambient);
    }
    
    game_mode_title_screen *Result = PushStruct(&GameState->ModeArena, game_mode_title_screen);
    Result->t = 0;

    GameState->TitleScreen = Result;
}

inline v3
CheckForInput(render_group *RenderGroup, object_transform *Transform, r32 Height, v3 Offset, r32 CAlign,
              game_input *Input, v3 MouseP, bitmap_id ID)
{
    v3 Result = {};
    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, ID, RenderGroup->GenerationID);
    if(Bitmap)
    {
        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, Transform, Bitmap, Height, Offset, CAlign);
        rectangle2 HoverRect = RectCenterDim(Dim.P.xy + 0.5f*Dim.Size, Dim.Size);
        v4 Color = V4(0, 0, 1, 1);
        if(IsInRectangle(HoverRect, MouseP.xy))
        {
            Color = V4(1, 0, 0, 1);
            Result = V3(0.5f, 0.0f, 0.5f);
        }

//        PushRectOutline(RenderGroup, Transform, HoverRect, 0.0f, Color, 0.03f);
    }
    
    return(Result);
}

internal b32
DrawStartButton(game_assets *Assets, render_group *RenderGroup, game_input *Input)
{
    b32 Result = false;
    if(RenderGroup)
    {
        object_transform Transform = DefaultFlatTransform();

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_Variety] = VarietyType_3;
        WeightVector.E[Tag_Variety] = 1;
        bitmap_id TitleImage = GetBestMatchBitmapFrom(Assets, Asset_TitleImage, &MatchVector, &WeightVector);
            
        Transform.OffsetP = V3(-8.0f, -3.0f, -9.0f);
        v2 MouseP = V2(Input->MouseX, Input->MouseY);
        v3 LocalMouseP = Unproject(RenderGroup, &Transform, MouseP);
        v3 AdditionalOffset = CheckForInput(RenderGroup, &Transform, 1.0f, V3(0, 0, 0), 1.0f,
                                            Input, LocalMouseP, TitleImage);
        if(AdditionalOffset.x || AdditionalOffset.y || AdditionalOffset.z)
        {
            Result = true;
        }
        
        Transform.OffsetP += AdditionalOffset;
        PushBitmap(RenderGroup, &Transform, TitleImage, 1.0f, V3(0, 0, 0));
    }

    return(Result);
}

internal b32
DrawExitButton(game_assets *Assets, render_group *RenderGroup, game_input *Input)
{
    b32 Result = false;
    if(RenderGroup)
    {
        object_transform Transform = DefaultFlatTransform();

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_Variety] = VarietyType_4;
        WeightVector.E[Tag_Variety] = 1;
        bitmap_id TitleImage = GetBestMatchBitmapFrom(Assets, Asset_TitleImage, &MatchVector, &WeightVector);

        Transform.OffsetP = V3(-8.0f, -4.0f, -9.0f);
        v2 MouseP = V2(Input->MouseX, Input->MouseY);
        v3 LocalMouseP = Unproject(RenderGroup, &Transform, MouseP);
        v3 AdditionalOffset = CheckForInput(RenderGroup, &Transform, 1.0f, V3(0, 0, 0), 1.0f,
                                            Input, LocalMouseP, TitleImage);
        if(AdditionalOffset.x || AdditionalOffset.y || AdditionalOffset.z)
        {
            Result = true;
        }
            
        Transform.OffsetP += AdditionalOffset;
        PushBitmap(RenderGroup, &Transform, TitleImage, 1.0f, V3(0, 0, 0));
    }

    return(Result);
}

internal void
DrawTitleScreen(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *Test)
{
    if(RenderGroup)
    {
        object_transform Transform = DefaultFlatTransform();

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_Variety] = VarietyType_0;
        WeightVector.E[Tag_Variety] = 1;
        bitmap_id TitleImage = GetBestMatchBitmapFrom(Assets, Asset_TitleImage, &MatchVector, &WeightVector);
            
        Clear(RenderGroup, V4(0, 0, 0, 1.0f));
        Transform.OffsetP = V3(0, 0, -10.0f);
        PushBitmap(RenderGroup, &Transform, TitleImage, 15.0f, V3(0, 0, 0));

        MatchVector.E[Tag_Variety] = VarietyType_1;
        Transform.OffsetP = V3(-4.5f, 5.0f, -9.0f);
        TitleImage = GetBestMatchBitmapFrom(Assets, Asset_TitleImage, &MatchVector, &WeightVector);
        PushBitmap(RenderGroup, &Transform, TitleImage, 3.0f, V3(0, 0, 0));

        MatchVector.E[Tag_Variety] = VarietyType_2;
        Transform.OffsetP = V3(-4.5f, 4.0f, -9.0f);
        TitleImage = GetBestMatchBitmapFrom(Assets, Asset_TitleImage, &MatchVector, &WeightVector);
        PushBitmap(RenderGroup, &Transform, TitleImage, 1.5f, V3(0, 0, 0));
    }
}

internal b32
UpdateAndRenderTitleScreen(game_state *GameState, transient_state *TranState, render_group *RenderGroup,
                           loaded_bitmap *DrawBuffer, game_input *Input, game_mode_title_screen *TitleScreen)
{
    game_assets *Assets = TranState->Assets;
    b32 Result = CheckForMetaInput(GameState, TranState, Input);
    if(!Result)
    {
        real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
        real32 MetersToPixels = (real32)DrawBuffer->Width*WidthOfMonitor;
        real32 FocalLength = 0.6f;

        Perspective(RenderGroup, MetersToPixels, FocalLength, 0.0f);

        DrawTitleScreen(Assets, RenderGroup, &TranState->MiniMap);

        if(DrawStartButton(Assets, RenderGroup, Input))
        {
            if(WasPressed(Input->MouseButtons[0]))
            {
                PlaySound(&GameState->AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_Click));
                GameState->FadeState = FadeState_FadeIn;
                GameState->GameHaveStarted = true;
            }
        }
        if(DrawExitButton(Assets, RenderGroup, Input))
        {
            if(WasPressed(Input->MouseButtons[0]))
            {
                PlaySound(&GameState->AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_Click));
                GameState->FadeState = FadeState_FadeIn;
                TitleScreen->Quit = true;
            }
        }

        if(GameState->GameHaveStarted && (GameState->CurrentAlpha == 1.0f))
        {
            GameState->AudioState.MasterVolume = V2(0, 0);
            PlayWorld(GameState, TranState);
            GameState->AudioState.MasterVolume = V2(0.5f, 0.5f);
        }
        else if(TitleScreen->Quit && (GameState->CurrentAlpha == 1.0f))
        {
            Input->QuitRequested = true;
        }
    }

    return(Result);
}
