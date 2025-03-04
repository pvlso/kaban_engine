/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
InitActionStack(action_stack *Stack, memory_arena *Arena)
{
    Stack->Actions = PushArray(Arena, MAX_UNDO_ACTIONS, editor_action);
    Stack->Start = 0;
    Stack->End = 0;
    Stack->Size = 0;
}

inline b32
IsActionStackEmpty(action_stack *Stack)
{
    b32 Result = (Stack->Size == 0);
    return(Result);
}

internal void
PushOnActionStack(action_stack *Stack, editor_action Action)
{
    Stack->Actions[Stack->End] = Action;
    Stack->End = (Stack->End + 1) % MAX_UNDO_ACTIONS;

    if(Stack->Size < MAX_UNDO_ACTIONS)
    {
        ++Stack->Size;
    }
    else
    {
        Stack->Start = (Stack->Start + 1) % MAX_UNDO_ACTIONS;
    }
}

internal editor_action *
PopFromActionStack(action_stack *Stack)
{
    editor_action *Result = 0;
    if(!IsActionStackEmpty(Stack))
    {
        Stack->End = (Stack->End - 1 + MAX_UNDO_ACTIONS) % MAX_UNDO_ACTIONS;
        --Stack->Size;

        Result = Stack->Actions + Stack->End;
    }

    return(Result);
}

inline void
ClearStack(action_stack *Stack)
{
    ZeroArray(MAX_UNDO_ACTIONS, Stack->Actions);
    Stack->Start = 0;
    Stack->End = 0;
    Stack->Size = 0;
}

internal void
UndoTileChanges(world *World, action_stack *UndoStack, action_stack *RedoStack)
{
    if(!IsActionStackEmpty(UndoStack))
    {
        editor_action *Action = PopFromActionStack(UndoStack);
        editor_action RedoAction = {};
        RedoAction.Type = Action->Type;
        switch(Action->Type)
        {
            case Action_None:
            {
                Assert(!"Action type shoud be asigned!");
            } break;
            
            case Action_AddTile:
            {
                sswm_ground_tile *Tile = GetWorldMapGroundTile(World, Action->PrevTile.TileX, Action->PrevTile.TileY);
                Copy(sizeof(sswm_ground_tile), Tile, &RedoAction.PrevTile);

                Copy(sizeof(sswm_ground_tile), &Action->PrevTile, Tile);
            } break;

            case Action_RemoveTile:
            {
                sswm_ground_tile *Tile = GetWorldMapGroundTile(World, Action->PrevTile.TileX, Action->PrevTile.TileY);
                Copy(sizeof(sswm_ground_tile), Tile, &RedoAction.PrevTile);

                Copy(sizeof(sswm_ground_tile), &Action->PrevTile, Tile);
            } break;
 
            case Action_FloodFill:
            {
                flood_fill_action *FloodFill = &Action->FloodFill;
                RedoAction.FloodFill.TileCount = FloodFill->TileCount;
                RedoAction.FloodFill.PrevTiles = (sswm_ground_tile *)Platform.AllocateMemory(MAX_FILL_TILES*sizeof(sswm_ground_tile));
                for(u32 PrevTileIndex = 0;
                    PrevTileIndex < FloodFill->TileCount;
                    ++PrevTileIndex)
                {
                    sswm_ground_tile *Tile = GetWorldMapGroundTile(World, FloodFill->PrevTiles[PrevTileIndex].TileX,
                                                                   FloodFill->PrevTiles[PrevTileIndex].TileY);
                    Copy(sizeof(sswm_ground_tile), Tile, RedoAction.FloodFill.PrevTiles + PrevTileIndex);
                    Copy(sizeof(sswm_ground_tile), FloodFill->PrevTiles + PrevTileIndex, Tile);
                }

                Platform.DeallocateMemory(FloodFill->PrevTiles);
            } break;

            InvalidDefaultCase;
        }
        
        PushOnActionStack(RedoStack, RedoAction);
    }
}

internal void
RedoTileChanges(world *World, action_stack *UndoStack, action_stack *RedoStack)
{
    if(!IsActionStackEmpty(RedoStack))
    {
        editor_action *Action = PopFromActionStack(RedoStack);
        editor_action UndoAction = {};
        UndoAction.Type = Action->Type;
        switch(Action->Type)
        {
            case Action_None:
            {
                Assert(!"Action type shoud be asigned!");
            } break;
            
            case Action_AddTile:
            {
                sswm_ground_tile *Tile = GetWorldMapGroundTile(World, Action->PrevTile.TileX, Action->PrevTile.TileY);
                Copy(sizeof(sswm_ground_tile), Tile, &UndoAction.PrevTile);

                Copy(sizeof(sswm_ground_tile), &Action->PrevTile, Tile);
            } break;

            case Action_RemoveTile:
            {
                sswm_ground_tile *Tile = GetWorldMapGroundTile(World, Action->PrevTile.TileX, Action->PrevTile.TileY);
                Copy(sizeof(sswm_ground_tile), Tile, &UndoAction.PrevTile);

                Copy(sizeof(sswm_ground_tile), &Action->PrevTile, Tile);
            } break;
 
            case Action_FloodFill:
            {
                flood_fill_action *FloodFill = &Action->FloodFill;
                UndoAction.FloodFill.TileCount = FloodFill->TileCount;
                UndoAction.FloodFill.PrevTiles = (sswm_ground_tile *)Platform.AllocateMemory(MAX_FILL_TILES*sizeof(sswm_ground_tile));
                for(u32 PrevTileIndex = 0;
                    PrevTileIndex < FloodFill->TileCount;
                    ++PrevTileIndex)
                {
                    sswm_ground_tile *Tile = GetWorldMapGroundTile(World, FloodFill->PrevTiles[PrevTileIndex].TileX,
                                                                   FloodFill->PrevTiles[PrevTileIndex].TileY);

                    Copy(sizeof(sswm_ground_tile), Tile, UndoAction.FloodFill.PrevTiles + PrevTileIndex);
                    Copy(sizeof(sswm_ground_tile), FloodFill->PrevTiles + PrevTileIndex, Tile);
                }

                Platform.DeallocateMemory(FloodFill->PrevTiles);
            } break;

            InvalidDefaultCase;
        }
        
        PushOnActionStack(UndoStack, UndoAction);
    }
}
