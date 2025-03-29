/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

enum vertex_flags
{
    VertexFlag_Outside = 1,
    VertexFlag_Inside = 2,
    VertexFlag_OnEdge = 3,
    VertexFlag_OnVertex = 4,
};

inline b32
EqualTo(u8 A, u8 TestA, u8 TestB, u8 TestC)
{
    b32 Result = ((A == TestA) || (A == TestB) || (A == TestC));
    return(Result);
}

global_variable s32 UniqueCount;
global_variable u8 TriangleUniqueTable[20][3];
global_variable s32 TriangleCount_;
global_variable u8 TriangleTable[64][3];

internal void
GenerateVertexTable()
{
    s32 VertexCount = 3;
    s32 StateCount = 4;
    u8 States[4] = {VertexFlag_Outside, VertexFlag_Inside, VertexFlag_OnEdge, VertexFlag_OnVertex};
    for(s32 A = 0;
        A < StateCount;
        ++A)
    {
        u8 VertexA = States[A];
        for(s32 B = 0;
            B < StateCount;
            ++B)
        {
            u8 VertexB = States[B];
            for(s32 C = 0;
                C < StateCount;
                ++C)
            {
                u8 VertexC = States[C];

                TriangleTable[TriangleCount_][0] = VertexA;
                TriangleTable[TriangleCount_][1] = VertexB;
                TriangleTable[TriangleCount_][2] = VertexC;
                ++TriangleCount_;
            }
        }
    }

    // NOTE(paul): Enable to record only unique cases
#if 0
    for(s32 I = 0;
        I < 64;
        ++I)
    {
        u8 StateCounts0[5] = {};
        u8 A = TriangleTable[I][0];
        u8 B = TriangleTable[I][1];
        u8 C = TriangleTable[I][2];
        ++StateCounts0[A];
        ++StateCounts0[B];
        ++StateCounts0[C];

        for(s32 J = I + 1;
            J < 64;
            ++J)
        {
            u8 StateCounts1[5] = {};
            u8 A0 = TriangleTable[J][0];
            u8 B0 = TriangleTable[J][1];
            u8 C0 = TriangleTable[J][2];
            ++StateCounts1[A0];
            ++StateCounts1[B0];
            ++StateCounts1[C0];

            if((StateCounts0[1] == StateCounts1[1]) && (StateCounts0[2] == StateCounts1[2]) &&
               (StateCounts0[3] == StateCounts1[3]) && (StateCounts0[4] == StateCounts1[4]))
            {
                TriangleTable[J][0] = 0;
                TriangleTable[J][1] = 0;
                TriangleTable[J][2] = 0;
            }
        }
    }

    for(s32 I = 0;
        I < 64;
        ++I)
    {
        u8 A = TriangleTable[I][0];
        u8 B = TriangleTable[I][1];
        u8 C = TriangleTable[I][2];
        if((A != 0) && (B != 0) && (C != 0))
        {
            TriangleUniqueTable[UniqueCount][0] = A;
            TriangleUniqueTable[UniqueCount][1] = B;
            TriangleUniqueTable[UniqueCount][2] = C;
            ++UniqueCount;
        }
    }
#endif
}

inline v2
GenerateOutsidePoint(triangle *T)
{
    r32 Margin = 0.5f;
    v2 p = {};
    
    if(rand() % 2)
    {
        p.x = T->Bounds.Min.x - Margin - (r32)(rand() % 4);
    }
    else
    {
        p.x = T->Bounds.Max.x + Margin + (r32)(rand() % 4);
    }
    
    if(rand() % 2)
    {
        p.y = T->Bounds.Min.y - Margin - (r32)(rand() % 4);
    }
    else
    {
        p.y = T->Bounds.Max.y + Margin + (r32)(rand() % 4);
    }

    return(p);
}

inline v2
GenerateInsidePoint(triangle *T)
{
    v2 p = {};

    v2 a = T->Vertices[1] - T->Vertices[0];
    v2 b = T->Vertices[2] - T->Vertices[0];
    r32 u1 = (r32)rand() / RAND_MAX;
    r32 u2 = (r32)rand() / RAND_MAX;
    if((u1 + u2) > 1.0f)
    {
        u1 = 1.0f - u1;
        u2 = 1.0f - u2;
    }

    v2 w = u1*a + u2*b;
    p = w + T->Vertices[0];
    
    return(p);
}

inline v2
GeneratePointOnEdge(triangle *T)
{
    v2 p;
    u32 Edge = rand() % 3;

    v2 p1 = T->Vertices[Edge];
    v2 p2 = T->Vertices[(Edge + 1) % 3];
    
    r32 t = (r32)rand() / (r32)RAND_MAX;
    p = Lerp(p1, t, p2);

    return(p);
}

inline v2
GeneratePointOnVertex(triangle *T, s32 VertexIndex)
{
    v2 Result = T->Vertices[VertexIndex];
    return(Result);
}

inline triangle
GenerateTriangle(triangle *T, u8 *VertexStates)
{
    triangle Result = {};
    for(s32 I = 0;
        I < 3;
        ++I)
    {
        if(VertexStates[I] == VertexFlag_Outside)
        {
            Result.Vertices[I] = GenerateOutsidePoint(T);
        }
        else if(VertexStates[I] == VertexFlag_Inside)
        {
            Result.Vertices[I] = GenerateInsidePoint(T);
        }
        else if(VertexStates[I] == VertexFlag_OnEdge)
        {
            Result.Vertices[I] = GeneratePointOnEdge(T);
        }
        else if(VertexStates[I] == VertexFlag_OnVertex)
        {
            Result.Vertices[I] = GeneratePointOnVertex(T, I);
        }

        Result.Vertices[I].x = (r32)RoundReal32ToInt32(Result.Vertices[I].x*100000.0f) / 100000.0f;         
        Result.Vertices[I].y = (r32)RoundReal32ToInt32(Result.Vertices[I].y*100000.0f) / 100000.0f;         
    }
    CalculateTriangleBoundingBox(&Result);

    return(Result);
}

inline r32
RandomFloat(r32 Min, r32 Max)
{
    r32 Result = Min + ((r32)rand() / (r32)RAND_MAX) * (Max - Min);

    return(Result); 
}

inline void
GenerateTriangleInRect(triangle *T, rectangle2 Rect)
{
    T->Vertices[0].x = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.x, Rect.Max.x)*100000.0f) / 100000.0f;
    T->Vertices[0].y = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.y, Rect.Max.y)*100000.0f) / 100000.0f;

    T->Vertices[1].x = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.x, Rect.Max.x)*100000.0f) / 100000.0f;
    T->Vertices[1].y = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.y, Rect.Max.y)*100000.0f) / 100000.0f;

    T->Vertices[2].x = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.x, Rect.Max.x)*100000.0f) / 100000.0f;
    T->Vertices[2].y = (r32)RoundReal32ToInt32(RandomFloat(Rect.Min.y, Rect.Max.y)*100000.0f) / 100000.0f;
}

//#include "triangulate.cpp"

internal void
TestPolygonSubtract(ui_state *UIState, editor_mode_game *GameMode, render_group *RenderGroup, object_transform *Flat, r32 dt,
                    memory_arena *Arena, world_position *BaseP)
{
    if(GameMode->RunTestForSeconds > 0.0f)
    {
        rectangle2 Bounds = RectCenterDim(V2(-6.0f, 0.0f), V2(15.0f, 15.0f));

        entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->SubjectT.Vertices[0], 0.0f));
        v3 P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "0", 0.8f);
        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->SubjectT.Vertices[1], 0.0f));
        P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "1", 0.8f);
        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->SubjectT.Vertices[2], 0.0f));
        P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "2", 0.8f);

        char Buffer[1024];
        b32 FirstGo = false;
        char *StatesStrings[5] = {"-", "Outside", "Inside", "On Edge", "On Vertex"};

        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->ClipT.Vertices[0], 0.0f));
        P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "0", 0.8f);
        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->ClipT.Vertices[1], 0.0f));
        P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "1", 0.8f);
        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(GameMode->ClipT.Vertices[2], 0.0f));
        P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        UITextOutAt(UIState, P.xy, "2", 0.8f);

        u32 Length = (u32)FormatString(ArrayCount(Buffer), Buffer, "Case:%d. %s|%s|%s, Fail: %s, Count: %d Time Left: %.2f",
                                       GameMode->NextIndex, StatesStrings[GameMode->States[0]], StatesStrings[GameMode->States[1]],
                                       StatesStrings[GameMode->States[2]], GameMode->SubResult.Fail ? "false" : "true",
                                       GameMode->SubResult.Set.PolygonCount, GameMode->RunTestForSeconds);
        UITextOutAt(UIState, V2(0.0f, 500.0f), Buffer, 1.0f);

        FormatString(ArrayCount(Buffer), Buffer, "Time Left: %.2f Seconds", GameMode->RunTestForSeconds);
        UITextOutAt(UIState, V2(0.0f, 600.0f), Buffer, 1.0f);

        FormatString(ArrayCount(Buffer), Buffer, "Fail: %s", GameMode->SubResult.Fail ? "false" : "true");
        UITextOutAt(UIState, V2(500.0f, 600.0f), Buffer, 1.0f);

        if(GameMode->SubResult.Set.PolygonCount)
        {
            v2 DrawOffset = V2(9.0f, 0.0f);
//            v2 DrawOffset = V2(0.0f, 0.0f);
            for(s32 PolygonIndex = 0;
                PolygonIndex < GameMode->SubResult.Set.PolygonCount;
                ++PolygonIndex)
            {
                polygon2 *Polygon = GameMode->SubResult.Set.Polygons + PolygonIndex;

                v4 Color = V4(0, 1, 0, 1);
                r32 Z = 10.0f;
            
                s32 WriteCount = 0;
                for(s32 VertexIndex = 0;
                    VertexIndex < Polygon->VertexCount;
                    ++VertexIndex)
                {
                    v4 VertexColor = V4(1, 0, 0, 1);
                    v2 Delta = Polygon->Vertices[VertexIndex];
        
                    if(VertexIndex == 0)
                    {
                        VertexColor = V4(0, 0, 1, 1);
                        Z = 15.0f;
                    }

                    if(VertexIndex == (Polygon->VertexCount - 1))
                    {
                        VertexColor = V4(1, 0, 1, 1);
                    }

                    PushRect(RenderGroup, Flat, V3(Delta + DrawOffset, Z), V2(0.15f, 0.15f), VertexColor);
                }

                if(Polygon->VertexCount > 1)
                {
                    for(s32 VertexIndex = 0;
                        VertexIndex < (Polygon->VertexCount - 1);
                        ++VertexIndex)
                    {
                        v2 Delta0 = Polygon->Vertices[VertexIndex];
                        v2 Delta1 = Polygon->Vertices[VertexIndex + 1];

                        PushLine(RenderGroup, Flat, V3(Delta0 + DrawOffset, Z), V3(Delta1 + DrawOffset, Z), Color);
                    }

                    if(Polygon->VertexCount > 2)
                    {
                        v2 Delta0 = Polygon->Vertices[Polygon->VertexCount - 1];
                        v2 Delta1 = Polygon->Vertices[0];
                        PushLine(RenderGroup, Flat, V3(Delta0 + DrawOffset, Z), V3(Delta1 + DrawOffset, Z), Color);
                    }
                }
            }

            if(FirstGo)
            {
                fwrite("\n\n", 2, 1, GameMode->PolygonTestLog);
            }

            if((GameMode->RunTestForSeconds - dt) <= 0.0f)
            {
                fclose(GameMode->PolygonTestLog);
            }
        }

//        GameMode->SubjectT.Vertices[0] = V2(-3.0f, 0.0f);
//        GameMode->SubjectT.Vertices[1] = V2(4.0f, 3.0f);
//        GameMode->SubjectT.Vertices[2] = V2(3.0f, -4.0f);

//        GameMode->ClipT.Vertices[0] = V2(-3.0f, -2.0f);
//        GameMode->ClipT.Vertices[1] = V2(2.5f, 4.0f);
//        GameMode->ClipT.Vertices[2] = V2(5.0f, -3.0f);
//        GameMode->SubResult = SubtractTriangelsF32(&GameMode->SubjectT, &GameMode->ClipT, 6.25e-3f, 0.06f, 0.125f, Arena);
    
#if 1
        temporary_memory TempM = BeginTemporaryMemory(Arena);
//        polygon2 Polygon = {};
//        Polygon.VertexCount = 6;
//        Polygon.Vertices = PushArray(TempM.Arena, Polygon.VertexCount, v2);
//        Polygon.Vertices[0] = {0, 0};
//        Polygon.Vertices[1] = {10, 0};
//        Polygon.Vertices[2] = {10, 2.5};
//        Polygon.Vertices[3] = {5, 2.5};
//        Polygon.Vertices[4] = {5, 5};
//        Polygon.Vertices[5] = {0, 5};

        f32 OneOverThree = 1.0f / 3.0f;
        for(s32 I = 0;
            I < GameMode->PolySet.PolygonCount;
            ++I)
        {
            triangulate_result Result = DelaunayTriangulate(GameMode->PolySet.Polygons + I, Arena);

            for(s32 I = 0;
                I < Result.TriangleCount;
                ++I)
            {
                triangle T = Result.Triangles[I];

                v2 Center = OneOverThree*(T.Vertices[0] + T.Vertices[1] + T.Vertices[2]);
                entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(Center, 0.0f));
                v3 P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
                FormatString(ArrayCount(Buffer), Buffer, "%d", I);
                UITextOutAt(UIState, P.xy, Buffer, 1.0f);

                PushTriangle(RenderGroup, Flat, T, 5.0f, V4(DebugColorTable[I], 0.6f));
            }

            Platform.DeallocateMemory(Result.Triangles);
            Platform.DeallocateMemory(Result.Adjacencies);
        }

#if 0
        int numPoints=9;
        float pointCloud[9][2]={{2,2.6},
                                {-5,5},
                                {5,5},
                                {-2,3},
                                {3,1},
                                {-4,-1},
                                {1,-2},
                                {-6,-4},
                                {5,-4}};
        
        float (*points)[2]=(float (*)[2])PushSize(TempM.Arena, numPoints*2*sizeof(float));
        for (int i=0;i<numPoints;i++){
            points[i][0]=pointCloud[i][0];
            points[i][1]=pointCloud[i][1];
        }

        struct ARRAYLIST* constraintEdge=(struct ARRAYLIST*)PushSize(TempM.Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
        struct ARRAYLIST* constraintEdge2=(struct ARRAYLIST*)PushSize(TempM.Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
        struct ARRAYLIST* constraintEdge3=(struct ARRAYLIST*)PushSize(TempM.Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
        constraintEdge->next=constraintEdge2;
        constraintEdge2->next=constraintEdge3;
        constraintEdge->numVals=2;
        constraintEdge2->numVals=2;
        constraintEdge3->numVals=2;
        constraintEdge->array[0]=0;
        constraintEdge->array[1]=7;
        constraintEdge2->array[0]=0;
        constraintEdge2->array[1]=8;
        constraintEdge3->array[0]=0;
        constraintEdge3->array[1]=2;

        delaunayTriangulate(points,constraintEdge,numPoints, RenderGroup, Flat, TempM.Arena, UIState);
#endif
        EndTemporaryMemory(TempM);
#else
        temporary_memory TempM = BeginTemporaryMemory(Arena);
#if 0

        polygon2 Polygon = {};
        Polygon.Vertices = PushArray(TempM.Arena, GameMode->Polygons->VertexCount, v2);
        ConvertNavMeshPolygonToPolygon2(GameMode->World, BaseP, GameMode->Polygons, &Polygon);
        RemoveDublicatPoints(&Polygon);
        DelaunayTriangulate(UIState, RenderGroup, Flat, &Polygon, Arena);
        s32 Count = GameMode->PolygonCount;
        for(nav_mesh_plygon *Step = GameMode->Polygons;
            Step;
            Step = Step->Next)
        {
            polygon2 Polygon = {};
            Polygon.Vertices = PushArray(TempM.Arena, Step->VertexCount, v2);
            ConvertNavMeshPolygonToPolygon2(GameMode->World, BaseP, Step, &Polygon);
            RemoveDublicatPoints(&Polygon);

            if((Count != 14) && (Count != 8) && (Count != 4) && (Count != 3))
            {
                DelaunayTriangulate(UIState, RenderGroup, Flat, &Polygon, Arena);
            }
            --Count;
        }
#endif
        EndTemporaryMemory(TempM);
#endif
        
#if 1        
        if(GameMode->NextTriangle < 0.0f)
        {
// TEST this
            s32 Count = 0;
//            while(Count < 2048)
            {
                GenerateTriangleInRect(&GameMode->SubjectT, Bounds);
                CalculateTriangleBoundingBox(&GameMode->SubjectT);
                  
                GameMode->States[0] = TriangleTable[GameMode->NextIndex][0];        
                GameMode->States[1] = TriangleTable[GameMode->NextIndex][1];        
                GameMode->States[2] = TriangleTable[GameMode->NextIndex][2];        
                GameMode->NextIndex++;
                if(GameMode->NextIndex >= TriangleCount_)
                {
                    GameMode->NextIndex = 0;
                }

                GameMode->ClipT = GenerateTriangle(&GameMode->SubjectT, GameMode->States);

                for(s32 PolygonIndex = 0;
                    PolygonIndex < GameMode->SubResult.Set.PolygonCount;
                    ++PolygonIndex)
                {
                    polygon2 *Polygon = GameMode->SubResult.Set.Polygons + PolygonIndex;
                    if(Polygon->HasHole)
                    {
                        Platform.DeallocateMemory(Polygon->HoleVertices);
                        Polygon->HoleVertexCount = 0;
                    }

                    Platform.DeallocateMemory(Polygon->Vertices);
                }
                Platform.DeallocateMemory(GameMode->SubResult.Set.Polygons);

                GameMode->SubResult = SubtractTriangelsF32(&GameMode->SubjectT, &GameMode->ClipT, 6.25e-3f, 0.06f, 0.125f, Arena);
                
                if(!GameMode->SubResult.Fail)
                {
                    u32 Length = (u32)FormatString(ArrayCount(Buffer), Buffer, "// Case:%d. Time: %.2f, %s|%s|%s, Fail: %s, Count: %d\n",
                                                   GameMode->NextIndex, GameMode->RunTestForSeconds, StatesStrings[GameMode->States[0]],
                                                   StatesStrings[GameMode->States[1]], StatesStrings[GameMode->States[2]],
                                                   GameMode->SubResult.Fail ? "false" : "true",
                                                   GameMode->SubResult.Set.PolygonCount);
                    fwrite(Buffer, Length, 1, GameMode->PolygonTestLog);

                    Length = (u32)FormatString(ArrayCount(Buffer), Buffer, "{{{V2(%.15ff, %.15ff), V2(%.15ff, %.15ff), V2(%.15ff, %.15ff)}, {}}, ",
                                               GameMode->SubjectT.Vertices[0].x, GameMode->SubjectT.Vertices[0].y,
                                               GameMode->SubjectT.Vertices[1].x, GameMode->SubjectT.Vertices[1].y,
                                               GameMode->SubjectT.Vertices[2].x, GameMode->SubjectT.Vertices[2].y);
                    fwrite(Buffer, Length, 1, GameMode->PolygonTestLog);

                    Length = (u32)FormatString(ArrayCount(Buffer), Buffer, "{{V2(%.15ff, %.15ff), V2(%.15ff, %.15ff), V2(%.15ff, %.15ff)}, {}}},\n",
                                               GameMode->ClipT.Vertices[0].x, GameMode->ClipT.Vertices[0].y,
                                               GameMode->ClipT.Vertices[1].x, GameMode->ClipT.Vertices[1].y,
                                               GameMode->ClipT.Vertices[2].x, GameMode->ClipT.Vertices[2].y);
                    fwrite(Buffer, Length, 1, GameMode->PolygonTestLog);

                    FirstGo = true;
                }

                ++Count;
            }
            
            GameMode->NextTriangle = 0.0f;
        }

        PushTriangle(RenderGroup, Flat, GameMode->SubjectT, 1.0f, V4(0.0f, 0.5f, 0.5f, 1));
        PushTriangle(RenderGroup, Flat, GameMode->ClipT, 2.0f, V4(0.5f, 0.0f, 0.5f, 0.2f));
        PushRectOutline(RenderGroup, Flat, Bounds, 2.0f, V4(0, 1.0f, 1.0f, 1), 0.02f);
#endif
        if(GameMode->Pause)
        {
            GameMode->NextTriangle -= dt;
        }

        GameMode->RunTestForSeconds -= dt;
    }
}
