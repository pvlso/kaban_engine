/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal void
AddGroundTile(action_stack *UndoStack, world *World, world_position P, ssa_tile Tile, u32 ZLayer, b32 FloodFill)
{
    sswm_ground_tile *GroundTile = GetWorldMapGroundTile(World, P.TileX, P.TileY);
    if(GroundTile)
    {
        if(!FloodFill)
        {
            editor_action AddTileAction = {};
            AddTileAction.Type = Action_AddTile;
            AddTileAction.PrevTile = *GroundTile;
            PushOnActionStack(UndoStack, AddTileAction);
        }

        GroundTile->BitmapID[ZLayer] = Tile.BitmapID.Value;
        GroundTile->CheckSum[ZLayer] = Tile.CheckSum;
    }
}

inline void
AddGroundTile(editor_mode_game *GameMode, world *World, world_position MouseP, b32 FloodFill)
{
    AddGroundTile(&GameMode->UndoStack, World, MouseP, GameMode->Tile, GameMode->CurrentZLayer, FloodFill);
}

inline void
RemoveGroundTile(action_stack *UndoStack, editor_mode_game *GameMode, world *World, world_position MouseP)
{
    sswm_ground_tile *GroundTile = GetWorldMapGroundTile(World, MouseP.TileX, MouseP.TileY);

    if(GroundTile)
    {
        editor_action RemoveTileAction = {};
        RemoveTileAction.Type = Action_RemoveTile;
        RemoveTileAction.PrevTile = *GroundTile;
        PushOnActionStack(UndoStack, RemoveTileAction);
            
        GroundTile->BitmapID[GameMode->CurrentZLayer] = 0;
        GroundTile->CheckSum[GameMode->CurrentZLayer] = 0;
    }
}

inline void
EnQueue(tile_queue *Queue, world_position P)
{
    if(Queue->Rear < (MAX_FILL_TILES - 1))
    {
        Queue->Tiles[++Queue->Rear] = P;
    }
}

inline world_position
DeQueue(tile_queue *Queue)
{
    world_position Result = Queue->Tiles[Queue->Front++];
    return(Result);
}

inline b32
IsEmpty(tile_queue *Queue)
{
    b32 Result = (Queue->Front > Queue->Rear);
    return(Result);
}

internal void
TileFloodFill(action_stack *UndoStack, world *World, world_position StartP, ssa_tile NewTile, u32 ZLayer)
{
    sswm_ground_tile *TargetGroundTiles = GetWorldMapGroundTile(World, StartP);
    if(TargetGroundTiles)
    {
        u32 TargetChecksum = TargetGroundTiles->CheckSum[ZLayer];
        if(TargetChecksum != NewTile.CheckSum)
        {
            editor_action FloodFillAction = {};
            FloodFillAction.Type = Action_FloodFill;
            FloodFillAction.FloodFill.TileCount = 0;
            FloodFillAction.FloodFill.PrevTiles = (sswm_ground_tile *)Platform.AllocateMemory(MAX_FILL_TILES*sizeof(sswm_ground_tile));

            tile_queue Queue = {};
            Queue.Front = 0;
            Queue.Rear = -1;
            EnQueue(&Queue, StartP);

            while(!IsEmpty(&Queue))
            {
                world_position P = DeQueue(&Queue);

                sswm_ground_tile *GroundTile = GetWorldMapGroundTile(World, P);
                if(GroundTile)
                {
                    if(GroundTile->CheckSum[ZLayer] == TargetChecksum)
                    {
                        Copy(sizeof(sswm_ground_tile), GroundTile, FloodFillAction.FloodFill.PrevTiles + FloodFillAction.FloodFill.TileCount++);

                        AddGroundTile(UndoStack, World, P, NewTile, ZLayer, true);
                        
                        if(TileIsValid(World, P.TileX - 1, P.TileY))
                        {
                            EnQueue(&Queue, CenteredTilePoint(World, P.TileX - 1, P.TileY));
                        }

                        if(TileIsValid(World, P.TileX + 1, P.TileY))
                        {
                            EnQueue(&Queue, CenteredTilePoint(World, P.TileX + 1, P.TileY));
                        }

                        if(TileIsValid(World, P.TileX, P.TileY - 1))
                        {
                            EnQueue(&Queue, CenteredTilePoint(World, P.TileX, P.TileY - 1));
                        }

                        if(TileIsValid(World, P.TileX, P.TileY + 1))
                        {
                            EnQueue(&Queue, CenteredTilePoint(World, P.TileX, P.TileY + 1));
                        }
                    }
                }
            }

            PushOnActionStack(UndoStack, FloodFillAction);
        }
    }
}
