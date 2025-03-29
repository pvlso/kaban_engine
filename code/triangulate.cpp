/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal b32
IsPointInPolygon(render_group *RenderGroup, object_transform *Flat, v2 p3, v2 p4, polygon2 *Poly)
{
    b32 Result = false;

    s32 Count = 0;
    v2 p;
    for(s32 VertexIndex = 0;
        VertexIndex < Poly->VertexCount;
        ++VertexIndex)
    {
        v2 p1 = Poly->Vertices[VertexIndex];
        v2 p2 = Poly->Vertices[(VertexIndex + 1) % Poly->VertexCount];
        if(LineIntersect(p1, p2, p3, p4, &p) > 0)
        {
            ++Count;
        }

//        PushLine(RenderGroup, Flat, V3(p3, 20.0f), V3(p, 20.0f), V4(1, 1, 0, 1));
        PushLine(RenderGroup, Flat, V3(p1, 20.0f), V3(p2, 20.0f), V4(1, 1, 0, 1));
    }

    if(Count % 2)
    {
        Result = true;
    }
    
    return(Result);
}

global_variable char Letters[12][2] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"};
#define DRAW_T 1

#if 0
internal void
//DelaunayTriangulate(ui_state *UIState, render_group *RenderGroup, object_transform *Flat, v2 *Points, s32 PointCount, memory_arena *Arena)
DelaunayTriangulate(ui_state *UIState, render_group *RenderGroup, object_transform *Flat, polygon2 *Poly, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    s32 PointCount = Poly->VertexCount;
    v2 *Points = PushArray(TempMem.Arena, PointCount + 3, v2);
    Copy(sizeof(v2)*PointCount, Poly->Vertices, Points);

//    polygon2 Poly = {};
//    Poly.VertexCount = PointCount;
//    Poly.Vertices = PushArray(TempMem.Arena, PointCount, v2);
//    Copy(sizeof(v2)*PointCount, Points, Poly.Vertices);
    
#if 0
    char Buffer[16];
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
//        PushRect(RenderGroup, Flat, V3(Points[I], 20.0f), V2(0.15f, 0.15f), V4(0, 0, 0, 1));

        entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(Points[I], 0.0f));
        v3 P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        FormatString(ArrayCount(Buffer), Buffer, "%d", I);
        UITextOutAt(UIState, P.xy, Buffer, 1.0f);
    }
#endif    
    // find min and max boundaries of point cloud
    rectangle2 Bounds = InvertedInfinityRectangle2();
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        if(Points[I].x > Bounds.Max.x) Bounds.Max.x = Points[I].x;
        else if(Points[I].x < Bounds.Min.x) Bounds.Min.x = Points[I].x;

        if(Points[I].y > Bounds.Max.y) Bounds.Max.y = Points[I].y;
        else if(Points[I].y < Bounds.Min.y) Bounds.Min.y = Points[I].y;
    }

    // remap everything (preserving the aspect ratio) to between (0,0)-(1,1)
    v2 BoundsDim = GetDim(Bounds);
    f32 d = BoundsDim.y; // d=largest dimension
    if(BoundsDim.x > d) d = BoundsDim.x;
    f32 OneOverd = 1.0f / d;
    
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        Points[I] = OneOverd*(Points[I] - Bounds.Min);
    }

    // sort points by proximity
    s32 BinRowsCount = CeilReal32ToInt32((f32)pow(PointCount, 0.25f));
    s32 *Bins = PushArray(TempMem.Arena, PointCount, s32);
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        s32 p = (s32)(Points[I].y*BinRowsCount*0.999f); // bin row
        s32 q = (int)(Points[I].x*BinRowsCount*0.999f); // bin column
        if(p % 2) Bins[I] = (p + 1)*BinRowsCount - q;
        else Bins[I] = p*BinRowsCount + (q + 1);
    }

    s32 *PointOrder = PushArray(TempMem.Arena, Poly->VertexCount, s32);
    for(s32 I = 0; I < Poly->VertexCount; ++I)
        PointOrder[I] = I;

    s32 Key;
    // insertion sort
    for(s32 I = 1;
        I < PointCount;
        ++I)
    {
        Key = Bins[I];
        v2 Temp = Points[I];
        s32 TempI = PointOrder[I];
        s32 J = I - 1;
        while((J >= 0) && (Bins[J] > Key))
        {
            Bins[J + 1] = Bins[J];
            Points[J + 1] = Points[J];
            PointOrder[J + 1] = PointOrder[J];
            --J;
        }

        Bins[J + 1] = Key;
        Points[J + 1] = Temp;
        PointOrder[J + 1] = TempI;
    }

    // add big triangle around our point cloud
    Points[PointCount] = V2(-300.0f, -300.0f);
    Points[PointCount + 1] = V2(300.0f, -300.0f);
    Points[PointCount + 2] = V2(0.0f, 300.0f);
    PointCount += 3;

    // data structures required
    triangle2i *Triangles = PushArray(TempMem.Arena, 10*MAX_VERTEX_COUNT, triangle2i);
    Triangles[0].Vertices[0] = PointCount - 3;
    Triangles[0].Vertices[1] = PointCount - 2;
    Triangles[0].Vertices[2] = PointCount - 1;
    Triangles[0].Adjacencies[0] = -1;
    Triangles[0].Adjacencies[1] = -1;
    Triangles[0].Adjacencies[2] = -1;

    s32 TriangleCount = 1;
    s32 *TriangleStack = PushArray(TempMem.Arena, 10*(PointCount - 3), s32); // is this a big enough stack?
    s32 Tos = -1;

    // insert all points and triangulate one by one
    for(s32 CurrentPointIndex = 0;
        CurrentPointIndex < (PointCount - 3);
        ++CurrentPointIndex)
    {
        // find triangle T which contains points[i]
        s32 LastCreated = TriangleCount - 1; // last triangle created
        while(1)
        {
            if(PlanarPointWithinTriangle(Points[CurrentPointIndex], Points[Triangles[LastCreated].V1],
                                         Points[Triangles[LastCreated].V2], Points[Triangles[LastCreated].V3]))
            {
                TriangleCount += 2;
                // delete triangle T and replace it with three sub-triangles touching P 

                // vertices of new triangles
                s32 FirstNew = TriangleCount - 2;
                s32 SecondNew = TriangleCount - 1;
                Triangles[FirstNew].V1 = CurrentPointIndex;
                Triangles[FirstNew].V2 = Triangles[LastCreated].V2;
                Triangles[FirstNew].V3 = Triangles[LastCreated].V3;

                Triangles[SecondNew].V1 = CurrentPointIndex;
                Triangles[SecondNew].V2 = Triangles[LastCreated].V3;
                Triangles[SecondNew].V3 = Triangles[LastCreated].V1;

                // update adjacencies of triangles surrounding the old triangle
                // fix adjacency of A
                s32 Adj1 = Triangles[LastCreated].AdjV1V2;
                s32 Adj2 = Triangles[LastCreated].AdjV2V3;
                s32 Adj3 = Triangles[LastCreated].AdjV3V1;

                if (Adj1 >= 0)
                {
                    for(s32 M = 0;
                        M < 3;
                        ++M)
                    {
                        if(Triangles[Adj1].Adjacencies[M] == LastCreated)
                        {
                            Triangles[Adj1].Adjacencies[M] = LastCreated;
                            break;
                        }
                    }
                }

                if(Adj2 >= 0)
                {
                    for(s32 M = 0;
                        M < 3;
                        ++M)
                    {
                        if(Triangles[Adj2].Adjacencies[M] == LastCreated)
                        {
                            Triangles[Adj2].Adjacencies[M] = FirstNew;
                            break;
                        }
                    }
                }

                if(Adj3 >= 0)
                {
                    for(s32 M = 0;
                        M < 3;
                        ++M)
                    {
                        if(Triangles[Adj3].Adjacencies[M] == LastCreated)
                        {
                            Triangles[Adj3].Adjacencies[M] = SecondNew;
                            break;
                        }
                    }
                }

                // adjacencies of new triangles 
                Triangles[FirstNew].AdjV1V2 = LastCreated;
                Triangles[FirstNew].AdjV2V3 = Triangles[LastCreated].AdjV2V3;
                Triangles[FirstNew].AdjV3V1 = SecondNew;

                Triangles[SecondNew].AdjV1V2 = FirstNew;
                Triangles[SecondNew].AdjV2V3 = Triangles[LastCreated].AdjV3V1;
                Triangles[SecondNew].AdjV3V1 = LastCreated;

                // replace v3 of containing triangle with P and rotate to v1
                Triangles[LastCreated].V3 = Triangles[LastCreated].V2;
                Triangles[LastCreated].V2 = Triangles[LastCreated].V1;
                Triangles[LastCreated].V1 = CurrentPointIndex;

                // replace 1st and 3rd adjacencies of containing triangle with new triangles
                Triangles[LastCreated].AdjV2V3 = Triangles[LastCreated].AdjV1V2;
                Triangles[LastCreated].AdjV3V1 = FirstNew;
                Triangles[LastCreated].AdjV1V2 = SecondNew;

                // place each triangle containing P onto a stack, if the edge opposite P has an adjacent triangle
                if (Triangles[LastCreated].AdjV2V3 >= 0) TriangleStack[++Tos] = LastCreated;
                if (Triangles[FirstNew].AdjV2V3 >= 0) TriangleStack[++Tos] = FirstNew;
                if (Triangles[SecondNew].AdjV2V3 >=0 ) TriangleStack[++Tos]= SecondNew;

                while(Tos >= 0)
                {
                    // looping thru the stack
                    s32 L = TriangleStack[Tos--];
                    v2 Vertex1 = Points[Triangles[L].V3];
                    v2 Vertex2 = Points[Triangles[L].V2];

                    s32 OppVert = -1;
                    s32 OppVertID = -1;
                    for(s32 k = 0; k < 3; ++k)
                    {
                        if((Triangles[Triangles[L].AdjV2V3].Vertices[k] != Triangles[L].V2)
                           && (Triangles[Triangles[L].AdjV2V3].Vertices[k] != Triangles[L].V3))
                        {
                            OppVert = Triangles[Triangles[L].AdjV2V3].Vertices[k];
                            OppVertID = k;
                            break;
                        }
                    }

                    v2 Vertex3 = Points[OppVert];
                    v2 P = Points[CurrentPointIndex];
                    
                    // check if P in circumcircle of triangle on top of stack
                    f32 cosa = Inner(Vertex1 - Vertex3, Vertex2 - Vertex3);
                    f32 cosb = Inner(Vertex2 - P, Vertex1 - P);
                    f32 sina = Cross(Vertex1 - Vertex3, Vertex2 - Vertex3);
                    f32 sinb = Cross(Vertex2 - P, Vertex1 - P);

                    if(((cosa < 0) && (cosb < 0)) || ((-cosa*sinb) > (cosb*sina)))
                    {
                        // swap diagonal, and redo triangles L R A & C
                        // initial state:
                        s32 R = Triangles[L].AdjV2V3;
                        s32 C = Triangles[L].AdjV3V1;
                        s32 A = Triangles[R].Adjacencies[(OppVertID + 2) % 3];

                        // fix adjacency of A
                        if(A >= 0)
                            for(s32 m = 0; m < 3; m++)
                            {
                                if(Triangles[A].Adjacencies[m] == R) {Triangles[A].Adjacencies[m] = L; break;}
                            }

                        // fix adjacency of C
                        if(C >= 0)
                            for(s32 m = 0; m < 3; m++)
                            {
                                if(Triangles[C].Adjacencies[m] == L) {Triangles[C].Adjacencies[m] = R; break;}
                            }

                        // fix vertices and adjacency of R
                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[R].Vertices[m] == OppVert) {Triangles[R].Vertices[(m + 2) % 3] = CurrentPointIndex; break;}
                        }

                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[R].Adjacencies[m] == L) {Triangles[R].Adjacencies[m] = C; break;}
                        }

                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[R].Adjacencies[m] == A) {Triangles[R].Adjacencies[m] = L; break;}
                        }

                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[R].V1 != CurrentPointIndex)
                            {
                                s32 Temp1 = Triangles[R].V1;
                                s32 Temp2 = Triangles[R].AdjV1V2;

                                Triangles[R].V1 = Triangles[R].V2;
                                Triangles[R].V2 = Triangles[R].V3;
                                Triangles[R].V3 = Temp1;

                                Triangles[R].AdjV1V2 = Triangles[R].AdjV2V3;
                                Triangles[R].AdjV2V3 = Triangles[R].AdjV3V1;
                                Triangles[R].AdjV3V1 = Temp2;
                            }
                        }
                        
                        // fix vertices and adjacency of L
                        Triangles[L].V3 = OppVert;
                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[L].Adjacencies[m] == C) {Triangles[L].Adjacencies[m] = R; break;}
                        }

                        for(s32 m = 0; m < 3; m++)
                        {
                            if(Triangles[L].Adjacencies[m] == R) {Triangles[L].Adjacencies[m] = A; break;}
                        }

                        // add L and R to stack if they have triangles opposite P;
                        if(Triangles[L].AdjV2V3 >= 0) TriangleStack[++Tos] = L;
                        if(Triangles[R].AdjV2V3 >= 0) TriangleStack[++Tos] = R;
                    }
                }

                break;
            }
        
            // adjust j in the direction of target point ii
            v2 AB = Points[Triangles[LastCreated].V2] - Points[Triangles[LastCreated].V1];
            v2 BC = Points[Triangles[LastCreated].V3] - Points[Triangles[LastCreated].V2];
            v2 CA = Points[Triangles[LastCreated].V1] - Points[Triangles[LastCreated].V3];

            v2 AP = Points[CurrentPointIndex] - Points[Triangles[LastCreated].V1];
            v2 BP = Points[CurrentPointIndex] - Points[Triangles[LastCreated].V2];
            v2 CP = Points[CurrentPointIndex] - Points[Triangles[LastCreated].V3];

            v2 N1 = V2(AB.y, -AB.x);
            v2 N2 = V2(BC.y, -BC.x);
            v2 N3 = V2(CA.y, -CA.x);

            f32 S1 = Inner(AP, N1);
            f32 S2 = Inner(BP, N2);
            f32 S3 = Inner(CP, N3);

            if((S1 > 0) && (S1 >= S2) && (S1 >= S3)) LastCreated = Triangles[LastCreated].AdjV1V2;
            else if((S2 > 0) && (S2 >= S1) && (S2 >= S3)) LastCreated = Triangles[LastCreated].AdjV2V3;
            else if((S3 > 0) && (S3 >= S1) && (S3 >= S2)) LastCreated = Triangles[LastCreated].AdjV3V1;
        }
    }
    
    // count how many triangles we have that dont involve supertriangle vertices
    s32 nTFinal = TriangleCount;
    s32 *RenumberAdj = PushArray(TempMem.Arena, TriangleCount, s32);
    b32 *DeadTris = PushArray(TempMem.Arena, TriangleCount, b32);
    for(s32 i = 0; i < TriangleCount; i++)
    {
        if((Triangles[i].V1 >= (PointCount - 3)) ||
           (Triangles[i].V2 >= (PointCount - 3)) ||
           (Triangles[i].V3 >= (PointCount - 3)))
        {
            DeadTris[i] = 1;
            RenumberAdj[i] = TriangleCount - (nTFinal--);
        }
        else
        {
            RenumberAdj[i] = TriangleCount - nTFinal;
        }
    }

    // delete any triangles that contain the supertriangle vertices
    triangle2i *verts_final = PushArray(TempMem.Arena, nTFinal, triangle2i);
    adj_triangles *tris_final = PushArray(TempMem.Arena, nTFinal, adj_triangles);

    s32 index = 0;
    for(s32 i = 0; i < TriangleCount; i++)
    {
        if((Triangles[i].V1 < (PointCount - 3)) &&
           (Triangles[i].V2 < (PointCount - 3)) &&
           (Triangles[i].V3 < (PointCount - 3)))
        {
            verts_final[index] = Triangles[i];

            verts_final[index].AdjV1V2   = (1 - DeadTris[Triangles[i].AdjV1V2])*Triangles[i].AdjV1V2 - DeadTris[Triangles[i].AdjV1V2];
            verts_final[index].AdjV2V3   = (1 - DeadTris[Triangles[i].AdjV2V3])*Triangles[i].AdjV2V3 - DeadTris[Triangles[i].AdjV2V3];
            verts_final[index++].AdjV3V1 = (1 - DeadTris[Triangles[i].AdjV3V1])*Triangles[i].AdjV3V1 - DeadTris[Triangles[i].AdjV3V1];
        }

    }

    for(s32 i = 0; i < nTFinal; i++)
    {
        if (verts_final[i].AdjV1V2 >= 0)
            verts_final[i].AdjV1V2 -= RenumberAdj[verts_final[i].AdjV1V2];
        if (verts_final[i].AdjV2V3 >= 0)
            verts_final[i].AdjV2V3 -= RenumberAdj[verts_final[i].AdjV2V3];
        if (verts_final[i].AdjV3V1 >= 0)
            verts_final[i].AdjV3V1 -= RenumberAdj[verts_final[i].AdjV3V1];
    }

    // undo the mapping
    PointCount -= 3;
    if(BoundsDim.x > d)
        d = BoundsDim.x;

    for(s32 i = 0; i < PointCount; i++)
    {
        Points[i] = Points[i]*d + Bounds.Min;
    }
#if DRAW_T

    b32 *TestedArray = PushArray(TempMem.Arena, nTFinal, b32);
    b32 *Draw = PushArray(TempMem.Arena, nTFinal, b32);
    for(s32 I = 0; I < nTFinal; ++I)
        Draw[I] = true;

    f32 OneOverThree = 1.0f / 3.0f;
    f32 PArea = PolygonSignedArea(Poly);
    b32 Clockwise = (PArea < 0.0f);
    for(s32 i = 0; i < nTFinal; i++)
    {
        triangle2i Triangle = verts_final[i];
        triangle T = {};
        T.Vertices[0] = Points[Triangle.V1];
        T.Vertices[1] = Points[Triangle.V2];
        T.Vertices[2] = Points[Triangle.V3];

        v2 Center = OneOverThree*(T.Vertices[0] + T.Vertices[1] + T.Vertices[2]);

        b32 Tested = false;
        if(!TestedArray[i])
        {
            TIMED_BLOCK("Remove");

            for(s32 I = 0;
                I < 3;
                ++I)
            {
                s32 V1 = Triangle.Vertices[I];
                s32 V2 = Triangle.Vertices[(I + 1) % 3];
                s32 LeftOf = Clockwise ? -1 : 1;
                if((PointOrder[V2] == (PointOrder[V1] + 1) % PointCount))
                {
                    if(TRISUBLeftOf(Points[V2], Points[V1], Center) == LeftOf)
                    {
                        RemoveExternalTriangles(verts_final, TestedArray, Draw, PointOrder, PointCount, &Triangle, I, i);
                        Draw[i] = false;
                    }

                    TestedArray[i] = true;
                    break;
                }
                else if((PointOrder[V1] == (PointOrder[V2] + 1) % PointCount))
                {
                    if(TRISUBLeftOf(Points[V1], Points[V2], Center) == LeftOf)
                    {
                        RemoveExternalTriangles(verts_final, TestedArray, Draw, PointOrder, PointCount, &Triangle, I, i);
                        Draw[i] = false;
                    }

                    TestedArray[i] = true;
                    break;
                }
            }
        }

        if(Draw[i])
        {
            PushTriangle(RenderGroup, Flat, T, 5.0f, V4(DebugColorTable[i], 0.7f));
        }
    }
#endif    

    EndTemporaryMemory(TempMem);
}

#endif

#if 0
struct point_pair_list
{
    point_pair_list *Next;
    union
    {
        struct
        {
            s32 P0;
            s32 P1;
        };

        s32 E[2];
    };
};

inline void
SplitTriangle(triangle2i *TriangleArray, s32 SubjectIndex, s32 FirstNewIndex, s32 SecondNewIndex, s32 CurrentPointIndex)
{

    // vertices of new triangles
    triangle2i *Subject = TriangleArray + SubjectIndex;
    triangle2i *FirstNewT = TriangleArray + FirstNewIndex;
    triangle2i *SecondNewT = TriangleArray + SecondNewIndex;

    FirstNewT->V1 = CurrentPointIndex;
    FirstNewT->V2 = Subject->V2;
    FirstNewT->V3 = Subject->V3;

    SecondNewT->V1 = CurrentPointIndex;
    SecondNewT->V2 = Subject->V3;
    SecondNewT->V3 = Subject->V1;

    // update adjacencies of triangles surrounding the old triangle
    // fix adjacency of Subject(old) triangle
    s32 Adj1 = Subject->AdjV1V2;
    s32 Adj2 = Subject->AdjV2V3;
    s32 Adj3 = Subject->AdjV3V1;

    if(Adj1 >= 0)
    {
        if(TriangleArray[Adj1].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj1].AdjV1V2 = SubjectIndex;
        else if(TriangleArray[Adj1].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj1].AdjV2V3 = SubjectIndex;
        else if(TriangleArray[Adj1].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj1].AdjV3V1 = SubjectIndex;
    }

    if(Adj2 >= 0)
    {
        if(TriangleArray[Adj2].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj2].AdjV1V2 = FirstNewIndex;
        else if(TriangleArray[Adj2].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj2].AdjV2V3 = FirstNewIndex;
        else if(TriangleArray[Adj2].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj2].AdjV3V1 = FirstNewIndex;
    }

    if(Adj3 >= 0)
    {
        if(TriangleArray[Adj3].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj3].AdjV1V2 = SecondNewIndex;
        else if(TriangleArray[Adj3].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj3].AdjV2V3 = SecondNewIndex;
        else if(TriangleArray[Adj3].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj3].AdjV3V1 = SecondNewIndex;
    }

    // adjacencies of new triangles 
    FirstNewT->AdjV1V2 = SubjectIndex;
    FirstNewT->AdjV2V3 = Subject->AdjV2V3;
    FirstNewT->AdjV3V1 = SecondNewIndex;

    SecondNewT->AdjV1V2 = FirstNewIndex;
    SecondNewT->AdjV2V3 = Subject->AdjV3V1;
    SecondNewT->AdjV3V1 = SubjectIndex;

    // replace v3 of containing triangle with P and rotate to v1
    Subject->V3 = Subject->V2;
    Subject->V2 = Subject->V1;
    Subject->V1 = CurrentPointIndex;

    // replace 1st and 3rd adjacencies of containing triangle with new triangles
    Subject->AdjV2V3 = Subject->AdjV1V2;
    Subject->AdjV3V1 = FirstNewIndex;
    Subject->AdjV1V2 = SecondNewIndex;
}

inline void
AdjacenciesFindWhichEqualAndSetTo(triangle2i *Test, s32 Compare, s32 Set)
{
    /*
      NOTE(babykaban):
      The function checks all three adjacency entries (AdjV1V2, AdjV2V3, AdjV3V1) of the triangle it's called on.
      It searches for the adjacency value that equals the old neighbor's index (Compare).
      It replaces that old neighbor index with the new one (Set), reflecting the new connection after the flip.
    */

    for(s32 m = 0; m < 3; m++)
    {
        if(Test->Adjacencies[m] == Compare)
        {
            Test->Adjacencies[m] = Set;
            break;
        }
    }
}

inline rectangle2
CalculateBoundsForPoints(v2 *Points, s32 PointCount)
{
    rectangle2 Bounds = InvertedInfinityRectangle2();
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        if(Points[I].x > Bounds.Max.x) Bounds.Max.x = Points[I].x;
        else if(Points[I].x < Bounds.Min.x) Bounds.Min.x = Points[I].x;

        if(Points[I].y > Bounds.Max.y) Bounds.Max.y = Points[I].y;
        else if(Points[I].y < Bounds.Min.y) Bounds.Min.y = Points[I].y;
    }

    return(Bounds);
}

inline b32
pointDirectionFromLineSegment2D(v2 p1, v2 p2, v2 p3)
{
    b32 Result = ((p3.x - p1.x)*(p2.y - p1.y) + (p3.y - p1.y)*(p1.x - p2.x)) < 0.0f;

    return(Result);
}

inline b32
PointDirectionFromLineSegment2D(v2 p1, v2 p2, v2 p3)
{
    b32 Result = ((p3.x - p1.x)*(p2.y - p1.y) + (p3.y - p1.y)*(p1.x - p2.x)) < 0.0f;

    return(Result);
}

inline b32
lineSegmentsCross2D(v2 p1, v2 p2, v2 p3, v2 p4)
{
    v2 p12 = p2 - p1;
    v2 p23 = p3 - p2;
    v2 p24 = p4 - p2;

    f32 cp1 = Cross(p12, p23);
    f32 cp2 = Cross(p12, p24);

    if ((cp1*cp2) >= 0) // 1st orientation test proves no intersection possible
        return 0;

    v2 p34 = p4 - p3;
    v2 p41 = p1 - p4;
    v2 p42 = p2 - p4; // this is the opposite of v24 above, can consolidate for performance

    cp1 = Cross(p34, p41);
    cp2 = Cross(p34, p42);

    if ((cp1*cp2) >= 0) // 2nd orientation test proves no intersection possible
        return 0;

    return 1;

}

inline b32
QuadrilateralConvex2D(v2 p1, v2 p2, v2 p3, v2 p4)
{
    v2 sides[4]={p2 - p1, p3 - p2, p4 - p3, p1 - p4};

    f32 cp;
    b32 sign = false;
    for(s32 i = 0; i < 4;i++)
    {
        cp = Cross(sides[i], sides[(i + 1) % 4]);
        if(i == 0)
            sign = (cp > 0);
        else if(sign != (cp > 0))
            return 0;
    }

    return 1;
}

internal void
ConstrainedDelaunayTriangulate(render_group *RenderGroup, object_transform *Flat, polygon2 *Poly, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    point_pair_list *ConstraintEdges = PushStruct(TempMem.Arena, point_pair_list);
    ConstraintEdges->Next = 0;
    ConstraintEdges->P0 = 3;
    ConstraintEdges->P1 = 5;

#if 0
    point_pair_list *Current = ConstraintEdges;
    for(s32 VertexIndex = 0;
        VertexIndex < Poly->VertexCount;
        ++VertexIndex)
    {
        Current->P0 = VertexIndex;
        Current->P1 = (VertexIndex + 1) % Poly->VertexCount;

        if((VertexIndex + 1) < Poly->VertexCount)
        {
            Current->Next = PushStruct(TempMem.Arena, point_pair_list);
            Current = Current->Next;
        }
    }
#endif

    s32 PointCount = Poly->VertexCount;
    v2 *Points = PushArray(TempMem.Arena, PointCount + 10, v2);
    Copy(sizeof(v2)*PointCount, Poly->Vertices, Points);
    
    // NOTE(babykaban): Find boundaries of point cloud
    rectangle2 Bounds = CalculateBoundsForPoints(Points, PointCount);

    // NOTE(babykaban): Remap everything (preserving the aspect ratio) to between (0,0)-(1,1)
    v2 BoundsDim = GetDim(Bounds);
    f32 d = BoundsDim.y; // d=largest dimension
    if(BoundsDim.x > d) d = BoundsDim.x;
    f32 OneOverd = 1.0f / d;
    
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        Points[I] = OneOverd*(Points[I] - Bounds.Min);
    }

    // NOTE(babykaban): Sort points by proximity
    s32 *PointOrder = PushArray(TempMem.Arena, PointCount, s32);
    for(s32 I = 0; I < PointCount; I++)
        PointOrder[I] = I;

    s32 BinRowsCount = CeilReal32ToInt32((f32)pow(PointCount, 0.25f));
    s32 *Bins = PushArray(TempMem.Arena, PointCount, s32);
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        s32 p = (s32)(Points[I].y*BinRowsCount*0.999f); // bin row
        s32 q = (int)(Points[I].x*BinRowsCount*0.999f); // bin column
        if(p % 2) Bins[I] = (p + 1)*BinRowsCount - q;
        else Bins[I] = p*BinRowsCount + (q + 1);
    }

    // NOTE(babykaban): Insertion sort
    s32 Key;
    for(s32 I = 1;
        I < PointCount;
        ++I)
    {
        Key = Bins[I];
        v2 Temp = Points[I];
        s32 TempI = PointOrder[I];
        s32 J = I - 1;
        while((J >= 0) && (Bins[J] > Key))
        {
            Bins[J + 1] = Bins[J];
            Points[J + 1] = Points[J];
            PointOrder[J + 1] = PointOrder[J];
            --J;
        }

        Bins[J + 1] = Key;
        Points[J + 1] = Temp;
        PointOrder[J + 1] = TempI;
    }


    // NOTE(babykaban): Add big triangle around point cloud
    Points[PointCount] = V2(-100.0f, -100.0f);
    Points[PointCount + 1] = V2(100.0f, -100.0f);
    Points[PointCount + 2] = V2(0.0f, 100.0f);
    PointCount += 3;

    triangle2i *Triangles = PushArray(TempMem.Arena, 3*MAX_VERTEX_COUNT, triangle2i);
    Triangles[0].Vertices[0] = PointCount - 3;
    Triangles[0].Vertices[1] = PointCount - 2;
    Triangles[0].Vertices[2] = PointCount - 1;
    Triangles[0].Adjacencies[0] = -1;
    Triangles[0].Adjacencies[1] = -1;
    Triangles[0].Adjacencies[2] = -1;

    s32 TriangleCount = 1;
    s32 *TriangleStack = PushArray(TempMem.Arena, 2*(PointCount - 3), s32); // is this a big enough stack?
    s32 Tos = -1;

    // NOTE(babykaban): Insert all points and triangulate one by one
    for(s32 CurrentPointIndex = 0;
        CurrentPointIndex < (PointCount - 3);
        ++CurrentPointIndex)
    {
        // NOTE(babykaban): Find triangle T which contains Points[CurrentPointIndex]
        s32 LastCreatedIndex = TriangleCount - 1; // Last triangle created
        while(1)
        {
            triangle2i *LastCreatedT = Triangles + LastCreatedIndex; // Triangle T
            if(PlanarPointWithinTriangle(Points[CurrentPointIndex], Points[LastCreatedT->V1],
                                         Points[LastCreatedT->V2], Points[LastCreatedT->V3]))
            {
                TriangleCount += 2;

                s32 FirstNewIndex = TriangleCount - 2;
                s32 SecondNewIndex = TriangleCount - 1;

                // NOTE(babykaban): Delete triangle T and replace it with three sub-triangles touching P 
                SplitTriangle(Triangles, LastCreatedIndex, FirstNewIndex, SecondNewIndex, CurrentPointIndex);

                // NOTE(babykaban): Place each triangle containing P(Points[CurrentPointIndex]) onto a stack,
                // if the edge opposite P(Points[CurrentPointIndex]) has an adjacent triangle
                if(LastCreatedT->AdjV2V3 >= 0)
                    TriangleStack[++Tos] = LastCreatedIndex;
                if(Triangles[FirstNewIndex].AdjV2V3 >= 0)
                    TriangleStack[++Tos] = FirstNewIndex;
                if(Triangles[SecondNewIndex].AdjV2V3 >=0)
                    TriangleStack[++Tos]= SecondNewIndex;

                while(Tos >= 0)
                {
                    v2 P = Points[CurrentPointIndex];

                    // NOTE(babykaban): Looping thru the stack
                    s32 TestTIndex = TriangleStack[Tos--];
                    // NOTE(babykaban): TestT, triangle at the top of the stack that is being tested.
                    triangle2i *TestT = Triangles + TestTIndex;
                    v2 Vertex1 = Points[TestT->V3];
                    v2 Vertex2 = Points[TestT->V2];

                    s32 OpositeTIndex = TestT->AdjV2V3;
                    // NOTE(babykaban): OpositeT, triangle adjacent to TestT across the edge formed by Vertex1 and Vertex2.
                    triangle2i *OpositeT = Triangles + OpositeTIndex;

                    s32 OppVert = -1;
                    s32 OppVertID = -1;
                    for(s32 k = 0; k < 3; ++k)
                    {
                        if((OpositeT->Vertices[k] != TestT->V2)
                           && (OpositeT->Vertices[k] != TestT->V3))
                        {
                            OppVert = OpositeT->Vertices[k];
                            OppVertID = k;
                            break;
                        }
                    }

                    v2 Vertex3 = Points[OppVert];
                    
                    // NOTE(babykaban): Check if P in circumcircle of TestT
                    f32 cosa = Inner(Vertex1 - Vertex3, Vertex2 - Vertex3);
                    f32 cosb = Inner(Vertex2 - P, Vertex1 - P);
                    f32 sina = Cross(Vertex1 - Vertex3, Vertex2 - Vertex3);
                    f32 sinb = Cross(Vertex2 - P, Vertex1 - P);

                    if(((cosa < 0) && (cosb < 0)) || ((-cosa*sinb) > (cosb*sina)))
                    {
                        s32 AdjacencentToTestTIndex = TestT->AdjV3V1;
                        s32 AdjacencentToOpositeTIndex = OpositeT->Adjacencies[(OppVertID + 2) % 3]; // A

                        triangle2i *AdjacencentToTestT = Triangles + AdjacencentToTestTIndex;
                        triangle2i *AdjacencentToOpositeT = Triangles + AdjacencentToOpositeTIndex; // C
                        
                        // NOTE(babykaban): Fix adjacency of A
                        if(AdjacencentToOpositeTIndex >= 0)
                        {
                            AdjacenciesFindWhichEqualAndSetTo(AdjacencentToOpositeT, OpositeTIndex, TestTIndex);
                        }

                        // NOTE(babykaban): Fix adjacency of C
                        if(AdjacencentToTestTIndex >= 0)
                        {
                            AdjacenciesFindWhichEqualAndSetTo(AdjacencentToTestT, TestTIndex, OpositeTIndex);
                        }

                        // NOTE(babykaban): Fix vertices and adjacency of OpositeT
                        for(s32 m = 0; m < 3; m++)
                        {
                            if(OpositeT->Vertices[m] == OppVert)
                            {OpositeT->Vertices[(m + 2) % 3] = CurrentPointIndex; break;}
                        }
                        AdjacenciesFindWhichEqualAndSetTo(OpositeT, TestTIndex, AdjacencentToTestTIndex);
                        AdjacenciesFindWhichEqualAndSetTo(OpositeT, AdjacencentToOpositeTIndex, TestTIndex);

                        for(s32 m = 0; m < 3; m++)
                        {
                            if(OpositeT->V1 != CurrentPointIndex)
                            {
                                s32 Temp1 = OpositeT->V1;
                                s32 Temp2 = OpositeT->AdjV1V2;

                                OpositeT->V1 = OpositeT->V2;
                                OpositeT->V2 = OpositeT->V3;
                                OpositeT->V3 = Temp1;

                                OpositeT->AdjV1V2 = OpositeT->AdjV2V3;
                                OpositeT->AdjV2V3 = OpositeT->AdjV3V1;
                                OpositeT->AdjV3V1 = Temp2;
                            }
                        }
                        
                        // NOTE(babykaban): Fix vertices and adjacency of TestT
                        TestT->V3 = OppVert;
                        AdjacenciesFindWhichEqualAndSetTo(TestT, AdjacencentToTestTIndex, OpositeTIndex);
                        AdjacenciesFindWhichEqualAndSetTo(TestT, OpositeTIndex, AdjacencentToOpositeTIndex);

                        // NOTE(babykaban): Add TestT and OpositeT to stack if they have triangles opposite P;
                        if(TestT->AdjV2V3 >= 0) TriangleStack[++Tos] = TestTIndex;
                        if(OpositeT->AdjV2V3 >= 0) TriangleStack[++Tos] = OpositeTIndex;
                    }
                }

                break;
            }
        
            // NOTE(babykaban): Adjust LastCreatedIndex in the direction of target point CurrentPointIndex
            v2 AB = Points[LastCreatedT->V2] - Points[LastCreatedT->V1];
            v2 BC = Points[LastCreatedT->V3] - Points[LastCreatedT->V2];
            v2 CA = Points[LastCreatedT->V1] - Points[LastCreatedT->V3];

            v2 AP = Points[CurrentPointIndex] - Points[LastCreatedT->V1];
            v2 BP = Points[CurrentPointIndex] - Points[LastCreatedT->V2];
            v2 CP = Points[CurrentPointIndex] - Points[LastCreatedT->V3];

            v2 N1 = V2(AB.y, -AB.x);
            v2 N2 = V2(BC.y, -BC.x);
            v2 N3 = V2(CA.y, -CA.x);

            f32 S1 = Inner(AP, N1);
            f32 S2 = Inner(BP, N2);
            f32 S3 = Inner(CP, N3);

            if((S1 >= 0) && (S1 >= S2) && (S1 >= S3)) LastCreatedIndex = LastCreatedT->AdjV1V2;
            else if((S2 >= 0) && (S2 >= S1) && (S2 >= S3)) LastCreatedIndex = LastCreatedT->AdjV2V3;
            else if((S3 >= 0) && (S3 >= S1) && (S3 >= S2)) LastCreatedIndex = LastCreatedT->AdjV3V1;
        }
    }
    
    // count how many triangles we have that dont involve supertriangle vertices
    s32 nTFinal = TriangleCount;
    s32 *RenumberAdj = PushArray(TempMem.Arena, TriangleCount, s32);
    b32 *DeadTris = PushArray(TempMem.Arena, TriangleCount, b32);
    for(s32 i = 0; i < TriangleCount; i++)
    {
        if((Triangles[i].V1 >= (PointCount - 3)) ||
           (Triangles[i].V2 >= (PointCount - 3)) ||
           (Triangles[i].V3 >= (PointCount - 3)))
        {
            DeadTris[i] = 1;
            RenumberAdj[i] = TriangleCount - (nTFinal--);
        }
        else
        {
            RenumberAdj[i] = TriangleCount - nTFinal;
        }
    }

    // delete any triangles that contain the supertriangle vertices
    triangle2i *verts_final = PushArray(TempMem.Arena, nTFinal, triangle2i);
    PointCount -= 3;
    
    s32 index = 0;
    for(s32 i = 0; i < TriangleCount; i++)
    {
        if((Triangles[i].V1 < (PointCount)) &&
           (Triangles[i].V2 < (PointCount)) &&
           (Triangles[i].V3 < (PointCount)))
        {
            verts_final[index] = Triangles[i];

            verts_final[index].AdjV1V2   = (1 - DeadTris[Triangles[i].AdjV1V2])*Triangles[i].AdjV1V2 - DeadTris[Triangles[i].AdjV1V2];
            verts_final[index].AdjV2V3   = (1 - DeadTris[Triangles[i].AdjV2V3])*Triangles[i].AdjV2V3 - DeadTris[Triangles[i].AdjV2V3];
            verts_final[index++].AdjV3V1 = (1 - DeadTris[Triangles[i].AdjV3V1])*Triangles[i].AdjV3V1 - DeadTris[Triangles[i].AdjV3V1];
        }

    }

    for(s32 i = 0; i < nTFinal; i++)
    {
        if (verts_final[i].AdjV1V2 >= 0)
            verts_final[i].AdjV1V2 -= RenumberAdj[verts_final[i].AdjV1V2];
        if (verts_final[i].AdjV2V3 >= 0)
            verts_final[i].AdjV2V3 -= RenumberAdj[verts_final[i].AdjV2V3];
        if (verts_final[i].AdjV3V1 >= 0)
            verts_final[i].AdjV3V1 -= RenumberAdj[verts_final[i].AdjV3V1];
    }

    // start video 017

    point_pair_list *Iter = ConstraintEdges;
    while(Iter)
    {
        // loop thru all constraint edge loops
        for(s32 i = 0; i < 1; i++)
        {
            // loop thru edge in the constraint edge loop
            s32 vert1 = 0;
            s32 vert2 = 0;
            for(s32 j = 0; j < PointCount; j++)
            { 
                if(PointOrder[j] == Iter->E[i])
                    vert1 = j;
                if(PointOrder[j] == Iter->E[i+1])
                    vert2 = j;
            }
            
            // generate a list of triangles that each vertex is contained in, if we have a lot of constraint edges, it would be good to pull this out of the loop
            s32 *vert1_tris = PushArray(TempMem.Arena, PointCount + 3, s32); // should be long enough
            s32 *vert1_tris_ind = PushArray(TempMem.Arena, PointCount + 3, s32); // should be long enough
            s32 *vert2_tris = PushArray(TempMem.Arena, PointCount + 3, s32); // should be long enough
            s32 ind1 = 0;
            s32 ind2 = 0;

            for(s32 j = 0; j < nTFinal; j++)
            {
                // loop thru all triangles to see which triangles contains our important vertices
                if(verts_final[j].V1 == vert1){
                    vert1_tris_ind[ind1] = 0;
                    vert1_tris[ind1++] = j;
                }
                else if (verts_final[j].V2 == vert1){
                    vert1_tris_ind[ind1] = 1;
                    vert1_tris[ind1++] = j;
                }
                else if (verts_final[j].V3 == vert1){
                    vert1_tris_ind[ind1] = 2;
                    vert1_tris[ind1++] = j;
                }

                if ((verts_final[j].V1 == vert2) || (verts_final[j].V2 == vert2) || (verts_final[j].V3 == vert2))
                    vert2_tris[ind2++] = j;
            }

            b32 edgeFound=0;
            s32 triTraj = -1;
            s32 triVertexInd = -1;

            for(int j = 0; j < ind1; j++)
            {
                // look thru all triangles containing vert1, looking for vert2, or until you find an edge that intersects vert1-vert2
                if((verts_final[vert1_tris[j]].V1 == vert2) || (verts_final[vert1_tris[j]].V2 == vert2) ||
                   (verts_final[vert1_tris[j]].V3 == vert2))
                {
                    // exact edge found
                    edgeFound = 1;
                    break;
                }
                else if (pointDirectionFromLineSegment2D(Points[vert1], // is vert2 to the left of both edges adjacent to vert1 on triangle j
                                                         Points[verts_final[vert1_tris[j]].Vertices[(vert1_tris_ind[j]+1)%3]],
                                                         Points[vert2]) &&
                         pointDirectionFromLineSegment2D(Points[verts_final[vert1_tris[j]].Vertices[(vert1_tris_ind[j]+2)%3]],
                                                         Points[vert1],
                                                         Points[vert2]))
                {
                    triTraj = vert1_tris[j];
                    triVertexInd = vert1_tris_ind[j];
                    break;
                }
            }

            if(edgeFound){ // constraint edge already in vertex array, do nothing
            
            }
            else
            {
                // constraint edge NOT found in vertex array
                // start looking for intersections in triangle triTraj
                point_pair_list *Intersections = PushStruct(TempMem.Arena, point_pair_list);
                point_pair_list *Iter2 = Intersections;
                Iter2->Next = 0; // add the first intersection edge details
//                Iter2->numVals=-1;

                while (1)
                {
                    if ((verts_final[triTraj].V1 == vert2) || // we made it to the triangle containing vert2
                        (verts_final[triTraj].V2 == vert2) ||
                        (verts_final[triTraj].V3 == vert2))
                    {
                        break;
                    }

                    for (int j=0;j<3;j++){ // check if vert2 is to the right of any edge in the current triangle

                        if(!pointDirectionFromLineSegment2D(Points[verts_final[triTraj].Vertices[j]], // if it is, it's a candidate for intersection
                                                            Points[verts_final[triTraj].Vertices[(j+1)%3]], // because the entry edge will fail this test, so no double counting
                                                            Points[vert2]))
                        {
                            if (lineSegmentsCross2D(Points[vert1], Points[vert2],
                                                    Points[verts_final[triTraj].Vertices[j]],
                                                    Points[verts_final[triTraj].Vertices[(j+1)%3]]))
                            {
                                if((Iter2->P0 == 0) && (Iter2->P1 == 0))
                                {
                                    Iter2->Next = 0;
                                    Iter2->P0 = verts_final[triTraj].Vertices[j];
                                    Iter2->P1 = verts_final[triTraj].Vertices[(j+1)%3];
                                }
                                else
                                {
                                    point_pair_list *NextIntersection = PushStruct(TempMem.Arena, point_pair_list);;
                                    Iter2->Next = NextIntersection;
                                    Iter2 = Iter2->Next;

                                    Iter2->Next = 0;
                                    Iter2->P0 = verts_final[triTraj].Vertices[j];
                                    Iter2->P1 = verts_final[triTraj].Vertices[(j+1)%3];
                                }

                                triTraj = verts_final[triTraj].Adjacencies[j];
                                break;
                            }
                        }
                    }
                }

                // loop thru edges that need to be removed, can't save the triangle index because we are killing triangles :(
                point_pair_list *Iter3 = Intersections;
                point_pair_list *Iter99 = Intersections;
        
                s32 triangle0 = -1;
                s32 triangle1 = -1;

                // new edges that we are adding to the triangulation
                point_pair_list *NewEdges = PushStruct(TempMem.Arena, point_pair_list);
                point_pair_list *Iter4 = NewEdges;
//                newEdges->numVals=-1;

                while(Iter3)
                {
                    // find triangles that contain this edge (one forward, one backward)
                    s32 triangle0_other_ind = -1;
                    s32 triangle1_other_ind = -1;
                    for(s32 j = 0; j < nTFinal; j++)
                    {
                        if((verts_final[j].V1 == Iter3->P1) && (verts_final[j].V2 == Iter3->P0))
                        {
                            triangle1 = j;
                            triangle1_other_ind = 2;
                            break;
                        }                           

                        if((verts_final[j].V1 == Iter3->P1) && (verts_final[j].V3 == Iter3->P0))
                        {
                            triangle1 = j;
                            triangle1_other_ind = 0;
                            break;
                        }                           

                        if((verts_final[j].V3 == Iter3->P1) && (verts_final[j].V1 == Iter3->P0))
                        {
                            triangle1 = j;
                            triangle1_other_ind = 1;
                            break;
                        }                           
                    }

                    for(s32 j = 0; j < nTFinal; j++)
                    {
                        if((verts_final[j].V1 == Iter3->P0) && (verts_final[j].V2 == Iter3->P1))
                        {
                            triangle0 = j;
                            triangle0_other_ind = 2;
                            break;
                        }                           

                        if((verts_final[j].V2 == Iter3->P0) && (verts_final[j].V3 == Iter3->P1))
                        {
                            triangle0 = j;
                            triangle0_other_ind = 0;
                            break;
                        }                           

                        if((verts_final[j].V3 == Iter3->P0) && (verts_final[j].V1 == Iter3->P1))
                        {
                            triangle0 = j;
                            triangle0_other_ind = 1;
                            break;
                        }                           
                    }
                    
                    if(QuadrilateralConvex2D(Points[verts_final[triangle0].Vertices[(triangle0_other_ind + 2) % 3]],
                                             Points[verts_final[triangle0].Vertices[triangle0_other_ind]],
                                             Points[verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3]],
                                             Points[verts_final[triangle1].Vertices[triangle1_other_ind]]))
                    {

                        // triangle convex, swap diagonal
                        verts_final[triangle0].Vertices[(triangle0_other_ind + 2) % 3] = verts_final[triangle1].Vertices[triangle1_other_ind];
                        verts_final[triangle1].Vertices[(triangle1_other_ind + 2) % 3] = verts_final[triangle0].Vertices[triangle0_other_ind];

                        // fix primary triangle adjacencies
                        verts_final[triangle0].Adjacencies[(triangle0_other_ind + 1) % 3] = verts_final[triangle1].Adjacencies[(triangle1_other_ind + 2) % 3];
                        verts_final[triangle1].Adjacencies[(triangle1_other_ind + 1) % 3] = verts_final[triangle0].Adjacencies[(triangle0_other_ind + 2) % 3];


                        verts_final[triangle0].Adjacencies[(triangle0_other_ind + 2) % 3] = triangle1;
                        verts_final[triangle1].Adjacencies[(triangle1_other_ind + 2) % 3] = triangle0;

                        // fix 2 nearby triangle adjacencies
                        s32 tri2Fix0 = verts_final[triangle0].Adjacencies[triangle0_other_ind];
                        s32 tri2Fix1 = verts_final[triangle1].Adjacencies[triangle1_other_ind];
                        for(s32 m = 0; m < 3; m++)
                            if(verts_final[tri2Fix0].Adjacencies[m] == triangle0)
                            {
                                verts_final[tri2Fix0].Adjacencies[m] = triangle1;
                                break;
                            }

                        for (s32 m = 0; m < 3;m++)
                            if (verts_final[tri2Fix1].Adjacencies[m] == triangle1)
                            {
                                verts_final[tri2Fix1].Adjacencies[m] = triangle0;
                                break;
                            }

                        triangle0_other_ind++; // rotate around 1 vtx
                        triangle1_other_ind++; // rotate around 1 vtx



                        // if the new diagonal still intersects the constraint edge, add it to the list
                        if(lineSegmentsCross2D(Points[vert1], Points[vert2],
                                               Points[verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3]],
                                               Points[verts_final[triangle1].Vertices[(triangle1_other_ind + 1) % 3]]))
                        {
                            point_pair_list *NextIntersection = PushStruct(TempMem.Arena, point_pair_list);
                            Iter2->Next = NextIntersection;
                            Iter2 = Iter2->Next;
                            Iter2->Next = 0;

                            Iter2->P0 = verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3];
                            Iter2->P1 = verts_final[triangle1].Vertices[(triangle1_other_ind + 1) % 3];
                        }
                        else
                        {
                            // if diagonal doesnt intersect the constraint edge, add to newEdge list
                            if((Iter4->P0 == 0) && (Iter4->P1 == 0))
                            { // first new edge
                                Iter4->Next = 0;
                                Iter4->P0 = verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3];
                                Iter4->P1 = verts_final[triangle1].Vertices[(triangle1_other_ind + 1) % 3];
                            }
                            else
                            {
                                point_pair_list *NextEdge = PushStruct(TempMem.Arena, point_pair_list);
                                Iter4->Next = NextEdge;
                                Iter4 = Iter4->Next;
                                Iter4->Next = 0;

                                Iter4->P0 = verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3];
                                Iter4->P1 = verts_final[triangle1].Vertices[(triangle1_other_ind + 1) % 3];                          
                            }
                        }
                    }
                    else
                    {
                        // quad not convex, put diagonal on the end of the linked list
                        point_pair_list *iter5 = Intersections;
                        point_pair_list *prev5 = 0;

                        while(iter5->Next)
                        {
                            if(iter5 == Iter3)
                            { // remove the old diagonal from the linked list
                                if(prev5 == 0)
                                {
                                    Intersections = Iter3->Next; // free the skipped one? (leak here?)
                                }
                                else
                                {
                                    prev5->Next = Iter3->Next; // again free the skipped one? (leak here?)
                                }
                            }

                            prev5 = iter5;
                            iter5 = iter5->Next;
                        }

                        point_pair_list *nextIntersection = PushStruct(TempMem.Arena, point_pair_list);
                        iter5->Next = nextIntersection;
                        iter5 = iter5->Next;
                        iter5->Next = 0;

                        iter5->P0 =verts_final[triangle0].Vertices[(triangle0_other_ind + 1) % 3];
                        iter5->P1 =verts_final[triangle1].Vertices[(triangle1_other_ind + 1) % 3];
                    }

                    Iter3 = Iter3->Next;
                }
            
                int swaps = 1;
                while(swaps > 0)
                {
                    // until no further swaps take place
                    swaps=0;
                    // restore the delaunay triangulation on the newEdges
                    // loop over newEdges
                    point_pair_list *iter6 = NewEdges;
                    while(iter6)
                    {
                        // if the new edge is NOT the constraint edge
                        if(!(((iter6->P0 == vert1) && (iter6->P1 == vert2)) ||
                             ((iter6->P0 == vert2) && (iter6->P1 == vert1))))
                        {
                            // identify lone vertex indices for the triangles which share the new edge, can keep an edge adjacency array to speed this up
                            // find triangles that contain this edge (one forward, one backward)
                            for(s32 j = 0; j < nTFinal; j++)
                            {
                                if(((verts_final[j].V1 == iter6->P1) && (verts_final[j].V2 == iter6->P0)) ||
                                   ((verts_final[j].V2 == iter6->P1) && (verts_final[j].V3 == iter6->P0)) ||
                                   ((verts_final[j].V3 == iter6->P1) && (verts_final[j].V1 == iter6->P0)))
                                {
                                    triangle0 = j;
                                    break;
                                }
                            }

                            for(int j = 0; j < nTFinal; j++)
                            {
                                if(((verts_final[j].V1 == iter6->P0) && (verts_final[j].V2 == iter6->P1)) ||
                                   ((verts_final[j].V2 == iter6->P0) && (verts_final[j].V3 == iter6->P1)) ||
                                   ((verts_final[j].V3 == iter6->P0) && (verts_final[j].V1 == iter6->P1)))
                                {
                                    triangle1 = j;
                                    break;
                                }
                            }

                            s32 triangle0_lone_vtx_ind = -1;
                            s32 triangle1_lone_vtx_ind = -1;

                            for(int j=0;j<3;j++) // find the lone vertex in triangle0
                                if((verts_final[triangle0].Vertices[j] != iter6->P0) &&
                                   (verts_final[triangle0].Vertices[j] != iter6->P1))
                                {
                                    triangle0_lone_vtx_ind = j;
                                }

                            for(s32 j = 0; j < 3; j++) // find the lone vertex in triangle1
                                if((verts_final[triangle1].Vertices[j] != iter6->P0) &&
                                   (verts_final[triangle1].Vertices[j] != iter6->P1))
                                {
                                    triangle1_lone_vtx_ind = j;
                                }

                            // if delaunay condition is not satisfied for triangles that share this edge, swap the diagonal
                            v2 v1a = Points[verts_final[triangle0].Vertices[(triangle0_lone_vtx_ind)]];
                            v2 v1b = Points[verts_final[triangle0].Vertices[(triangle0_lone_vtx_ind + 1) % 3]];

                            v2 v1c = Points[verts_final[triangle0].Vertices[(triangle0_lone_vtx_ind + 2) % 3]];  
                            v2 v2a = Points[verts_final[triangle1].Vertices[(triangle1_lone_vtx_ind)]];

                            f32 cosa1 = Inner(v1b - v1a, v1c - v1a);
                            f32 cosa2 = Inner(v1c - v2a, v1b - v2a);
                            f32 cosb1 = Inner(v1c - v2a, v1b - v2a);
                            f32 cosb2 = Inner(v1b - v1a, v1c - v1a);

                            b32 C1 = (cosa1 < 0) && (cosb1 < 0);
                            b32 C2 = (-cosa1*Cross(v1c - v2a, v1b - v2a)) > (cosb1*Cross(v1b - v1a, v1c - v1a));
                            b32 C3 = (cosa2 < 0) && (cosb2 < 0);
                            b32 C4 = (-cosa2*Cross(v1b - v1a, v1c - v1a)) > (cosb2*Cross(v1c - v2a, v1b - v2a));
                            
                            if((C1 || C2) || (C3 || C4))
                            {
                                verts_final[triangle0].Vertices[(triangle0_lone_vtx_ind + 2) % 3] = verts_final[triangle1].Vertices[triangle1_lone_vtx_ind];
                                verts_final[triangle1].Vertices[(triangle1_lone_vtx_ind + 2) % 3] = verts_final[triangle0].Vertices[triangle0_lone_vtx_ind];

                                // update triangle0 and triangle1 adjacencies
                                verts_final[triangle0].Adjacencies[(triangle0_lone_vtx_ind + 1) % 3] =
                                    verts_final[triangle1].Adjacencies[(triangle1_lone_vtx_ind + 2) % 3];
                                verts_final[triangle1].Adjacencies[(triangle1_lone_vtx_ind + 1) % 3] =
                                    verts_final[triangle0].Adjacencies[(triangle0_lone_vtx_ind + 2) % 3];
                                verts_final[triangle0].Adjacencies[(triangle0_lone_vtx_ind + 2) % 3] = triangle1;
                                verts_final[triangle1].Adjacencies[(triangle1_lone_vtx_ind + 2) % 3] = triangle0;

                                // fix 2 nearby triangle adjacencies
                                s32 tri2Fix0 = verts_final[triangle0].Adjacencies[triangle0_lone_vtx_ind];
                                s32 tri2Fix1 = verts_final[triangle1].Adjacencies[triangle1_lone_vtx_ind];
                                for(s32 m = 0; m < 3; m++)
                                    if(verts_final[tri2Fix0].Adjacencies[m] == triangle0)
                                    {
                                        verts_final[tri2Fix0].Adjacencies[m] = triangle1;
                                        break;
                                    }

                                for(s32 m = 0; m < 3; m++)
                                    if (verts_final[tri2Fix1].Adjacencies[m] == triangle1)
                                    {
                                        verts_final[tri2Fix1].Adjacencies[m] = triangle0;
                                        break;
                                    }

                                iter6->P0 = verts_final[triangle0].Vertices[triangle0_lone_vtx_ind]; // swap diagonal in new edge list
                                iter6->P1 = verts_final[triangle1].Vertices[triangle1_lone_vtx_ind]; // swap diagonal in new edge list

                                triangle0_lone_vtx_ind++; // rotate the lone vtx +1
                                triangle1_lone_vtx_ind++; // rotate the lone vtx +1

                                swaps++; // keep track of number of swaps

                            }
                        }

                        iter6 = iter6->Next;
                    }
                }
            }
        }

        Iter = Iter->Next;
    }

    // end video 017

    // undo the mapping
    if(BoundsDim.x > d)
        d = BoundsDim.x;

    for(s32 i = 0; i < PointCount; i++)
    {
        Points[i] = Points[i]*d + Bounds.Min;
    }

#if DRAW_T
    for(s32 i = 0; i < nTFinal; i++)
    {
        triangle2i Triangle = verts_final[i];
        triangle T = {};
        T.Vertices[0] = Points[Triangle.V1];
        T.Vertices[1] = Points[Triangle.V2];
        T.Vertices[2] = Points[Triangle.V3];

        PushTriangle(RenderGroup, Flat, T, 5.0f, V4(DebugColorTable[i], 0.7f));
    }
#endif

    EndTemporaryMemory(TempMem);
}
#endif

bool planarPointWithinTriangle(float P[2],float V1[2],float V2[2],float V3[2]){
                    
    float AB[2]={V2[0]-V1[0],V2[1]-V1[1]};
    float BC[2]={V3[0]-V2[0],V3[1]-V2[1]};
    float CA[2]={V1[0]-V3[0],V1[1]-V3[1]};
    float AP[2]={P[0]-V1[0],P[1]-V1[1]};
    float BP[2]={P[0]-V2[0],P[1]-V2[1]};
    float CP[2]={P[0]-V3[0],P[1]-V3[1]};

    float N1[2]={AB[1],-AB[0]};
    float N2[2]={BC[1],-BC[0]};
    float N3[2]={CA[1],-CA[0]};
    
    float S1=AP[0]*N1[0]+AP[1]*N1[1];
    float S2=BP[0]*N2[0]+BP[1]*N2[1];
    float S3=CP[0]*N3[0]+CP[1]*N3[1];

    float tolerance=0.0001;

    if ((S1<0&&S2<0&&S3<0)||
        (S1<tolerance&&S2<0&&S3<0)||
        (S2<tolerance&&S1<0&&S3<0)||
        (S3<tolerance&&S1<0&&S2<0)){ // inside triangle
        return 1;   
    }
    else{
        return 0;
    }

}

// return 1 if p3 is left of p1->p2, 0 if right
bool pointDirectionFromLineSegment2D(float p1[2], float p2[2], float p3[2]){
    return ((p3[0]-p1[0])*(p2[1]-p1[1])+(p3[1]-p1[1])*(p1[0]-p2[0]))<0;
}

// return 1 if p1->p2 crosses p3->p4
bool lineSegmentsCross2D(float p1[2], float p2[2], float p3[2], float p4[2]){

    float v12[2]={p2[0]-p1[0],p2[1]-p1[1]};
    float v23[2]={p3[0]-p2[0],p3[1]-p2[1]};
    float v24[2]={p4[0]-p2[0],p4[1]-p2[1]};

    float cp1=v12[0]*v23[1]-v23[0]*v12[1];
    float cp2=v12[0]*v24[1]-v24[0]*v12[1];

    if ((cp1*cp2)>=0) // 1st orientation test proves no intersection possible
        return 0;

    float v34[2]={p4[0]-p3[0],p4[1]-p3[1]};
    float v41[2]={p1[0]-p4[0],p1[1]-p4[1]};
    float v42[2]={p2[0]-p4[0],p2[1]-p4[1]}; // this is the opposite of v24 above, can consolidate for performance

    cp1=v34[0]*v41[1]-v41[0]*v34[1];
    cp2=v34[0]*v42[1]-v42[0]*v34[1];

    if ((cp1*cp2)>=0) // 2nd orientation test proves no intersection possible
        return 0;

    return 1;

}

// returns 1 if quadrilateral p1-p2-p3-p4 is convex
bool quadrilateralConvex2D(float p1[2], float p2[2], float p3[2], float p4[2]){
    float sides[4][2]={{p2[0]-p1[0],p2[1]-p1[1]},
                        {p3[0]-p2[0],p3[1]-p2[1]},
                        {p4[0]-p3[0],p4[1]-p3[1]},
                        {p1[0]-p4[0],p1[1]-p4[1]}};
    float cp;
    bool sign=0;
    for (int i=0;i<4;i++){
        cp=sides[i][0]*sides[(i+1)%4][1]-sides[i][1]*sides[(i+1)%4][0];
        if (i==0)
            sign=(cp>0);
        else if (sign!=(cp>0))
            return 0;
    }
    return 1;
}

struct ARRAYLIST {
    struct ARRAYLIST* next;
    int numVals;
    int array[2];
};

void delaunayTriangulate(float points[][2], struct ARRAYLIST* constraintEdges, int numPoints, render_group *RenderGroup, object_transform *Flat,
                         memory_arena *Arena, ui_state *UIState){
    char Buffer[16];
    for(s32 I = 0;
        I < numPoints;
        ++I)
    {
        PushRect(RenderGroup, Flat, V3(points[I][0], points[I][1], 20.0f), V2(0.15f, 0.15f), V4(0, 0, 0, 1));

        entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, Flat, V3(points[I][0], points[I][1], 0.0f));
        v3 P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
        FormatString(ArrayCount(Buffer), Buffer, "%d", I);
        UITextOutAt(UIState, P.xy, Buffer, 1.0f);
    }
    
// find min and max boundaries of point cloud
    float xmin=0,xmax=0,ymin=0,ymax=0;
    for (int i=0;i<numPoints;i++){
        if (points[i][0]>xmax)
            xmax=points[i][0];
        else if (points[i][0]<xmin)
            xmin=points[i][0];
        if (points[i][1]>ymax)
            ymax=points[i][1];
        else if (points[i][1]<ymin)
            ymin=points[i][1];
    }

    // remap everything (preserving the aspect ratio) to between (0,0)-(1,1)
    float height=ymax-ymin;
    float width=xmax-xmin;
    float d=height; // d=largest dimension
    if (width>d)
        d=width;
    for (int i=0;i<numPoints;i++){
        points[i][0]=(points[i][0]-xmin)/d;
        points[i][1]=(points[i][1]-ymin)/d;
    }

    // sort points by proximity
    int *pointOrder = (int *)PushSize(Arena,sizeof(int)*numPoints); // 017
    for (int i=0;i<numPoints;i++)
        pointOrder[i]=i;
    int NbinRows=(int)ceil(pow(numPoints,0.25));
    int* bins= (int *)PushSize(Arena,numPoints*sizeof(int));
    for (int i=0;i<numPoints;i++){
        int p=(int)(points[i][1]*NbinRows*0.999); // bin row
        int q=(int)(points[i][0]*NbinRows*0.999); // bin column
        if (p%2)
            bins[i]=(p+1)*NbinRows-q;
        else
            bins[i]=p*NbinRows+q+1;
    }
    int key;
    for(int i=1; i<numPoints; i++){ // insertion sort
        key=bins[i];
        float tempF[2]={points[i][0],points[i][1]};
        int tempI=pointOrder[i];
        int j=i-1;
        while(j>=0&&(bins[j]>key)){
            bins[j+1]=bins[j];
            points[j+1][0]=points[j][0];points[j+1][1]=points[j][1];
            pointOrder[j+1]=pointOrder[j];
            j--;
        }
        bins[j+1]=key;
        points[j+1][0]=tempF[0];points[j+1][1]=tempF[1];
        pointOrder[j+1]=tempI;
    }

    // add big triangle around our point cloud
    points= (float (*)[2])PushCopy(Arena, (3+numPoints)*2*sizeof(float), points);
    points[numPoints][0]=-100;points[numPoints][1]=-100;
    points[numPoints+1][0]=100;points[numPoints+1][1]=-100;
    points[numPoints+2][0]=0;points[numPoints+2][1]=100;
    numPoints+=3;

    // data structures required
    int (*verts)[3]=(int (*)[3])PushSize(Arena,3*sizeof(int));
    verts[0][0]=numPoints-3;
    verts[0][1]=numPoints-2;
    verts[0][2]=numPoints-1;
    int (*tris)[3]=(int (*)[3])PushSize(Arena,3*sizeof(int));
    tris[0][0]=-1;
    tris[0][1]=-1;
    tris[0][2]=-1;
    int nT=1;
    int *triangleStack=(int *)PushSize(Arena,(numPoints-3)*sizeof(int)); // is this a big enough stack?
    int tos=-1;
    // insert all points and triangulate one by one
    for (int ii=0;ii<(numPoints-3);ii++){
        // find triangle T which contains points[i]
        int j=nT-1; // last triangle created
        while (1){
            if (planarPointWithinTriangle(points[ii],points[verts[j][0]],points[verts[j][1]],points[verts[j][2]])){
                nT+=2;
                // delete triangle T and replace it with three sub-triangles touching P 
                tris=(int (*)[3])PushCopy(Arena, (nT)*3*sizeof(int), tris);
                verts=(int (*)[3])PushCopy(Arena, (nT)*3*sizeof(int), verts);
                // vertices of new triangles
                verts[nT-2][0]=ii;
                verts[nT-2][1]=verts[j][1];
                verts[nT-2][2]=verts[j][2];
                verts[nT-1][0]=ii;
                verts[nT-1][1]=verts[j][2];
                verts[nT-1][2]=verts[j][0];
                // update adjacencies of triangles surrounding the old triangle
                // fix adjacency of A
                int adj1=tris[j][0];
                int adj2=tris[j][1];
                int adj3=tris[j][2];
                if (adj1>=0)
                    for (int m=0;m<3;m++)
                        if (tris[adj1][m]==j){
                            tris[adj1][m]=j;
                            break;
                        }
                if (adj2>=0)
                    for (int m=0;m<3;m++)
                        if (tris[adj2][m]==j){
                            tris[adj2][m]=nT-2;
                            break;
                        }
                if (adj3>=0)
                    for (int m=0;m<3;m++)
                        if (tris[adj3][m]==j){
                            tris[adj3][m]=nT-1;
                            break;
                        }
                // adjacencies of new triangles 
                tris[nT-2][0]=j;
                tris[nT-2][1]=tris[j][1];
                tris[nT-2][2]=nT-1;
                tris[nT-1][0]=nT-2;
                tris[nT-1][1]=tris[j][2];
                tris[nT-1][2]=j;
                // replace v3 of containing triangle with P and rotate to v1
                verts[j][2]=verts[j][1];
                verts[j][1]=verts[j][0];
                verts[j][0]=ii;
                // replace 1st and 3rd adjacencies of containing triangle with new triangles
                tris[j][1]=tris[j][0];
                tris[j][2]=nT-2;
                tris[j][0]=nT-1;
                // place each triangle containing P onto a stack, if the edge opposite P has an adjacent triangle
                if (tris[j][1]>=0)
                    triangleStack[++tos]=j;
                if (tris[nT-2][1]>=0)
                    triangleStack[++tos]=nT-2;
                if (tris[nT-1][1]>=0)
                    triangleStack[++tos]=nT-1;
                while (tos>=0){ // looping thru the stack
                    int L=triangleStack[tos--];
                    float v1[2]={points[verts[L][2]][0],points[verts[L][2]][1]};
                    float v2[2]={points[verts[L][1]][0],points[verts[L][1]][1]};
                    int oppVert=-1;
                    int oppVertID=-1;
                    for (int k=0;k<3;k++){
                        if ((verts[tris[L][1]][k]!=verts[L][1])
                           &&(verts[tris[L][1]][k]!=verts[L][2])){
                            oppVert=verts[tris[L][1]][k];
                            oppVertID=k;
                            break;
                        }
                    }
                    float v3[2]={points[oppVert][0],points[oppVert][1]};
                    float P[2]={points[ii][0],points[ii][1]};
                    
                    // check if P in circumcircle of triangle on top of stack
                    float cosa=((v1[0]-v3[0])*(v2[0]-v3[0])+(v1[1]-v3[1])*(v2[1]-v3[1]));
                    float cosb=((v2[0]-P[0])*(v1[0]-P[0])+(v2[1]-P[1])*(v1[1]-P[1]));
                    float sina=((v1[0]-v3[0])*(v2[1]-v3[1])-(v1[1]-v3[1])*(v2[0]-v3[0]));
                    float sinb=((v2[0]-P[0])*(v1[1]-P[1])-(v2[1]-P[1])*(v1[0]-P[0]));
                    
                    if (((cosa<0)&&(cosb<0))||
                        ((-cosa*((v2[0]-P[0])*(v1[1]-P[1])-(v2[1]-P[1])*(v1[0]-P[0])))>
                         (cosb*((v1[0]-v3[0])*(v2[1]-v3[1])-(v1[1]-v3[1])*(v2[0]-v3[0]))))){

                        // swap diagonal, and redo triangles L R A & C
                        // initial state:
                        int R=tris[L][1];
                        int C=tris[L][2];
                        int A=tris[R][(oppVertID+2)%3];
                        // fix adjacency of A
                        if (A>=0)
                            for (int m=0;m<3;m++)
                                if (tris[A][m]==R){
                                    tris[A][m]=L;
                                    break;
                                }
                        // fix adjacency of C
                        if (C>=0)
                            for (int m=0;m<3;m++)
                                if (tris[C][m]==L){
                                    tris[C][m]=R;
                                    break;
                                }
                        // fix vertices and adjacency of R
                        for (int m=0;m<3;m++)
                            if (verts[R][m]==oppVert){
                                verts[R][(m+2)%3]=ii;
                                break;
                            }
                        for (int m=0;m<3;m++)
                            if (tris[R][m]==L){
                                tris[R][m]=C;
                                break;
                            }
                        for (int m=0;m<3;m++)
                            if (tris[R][m]==A){
                                tris[R][m]=L;
                                break;
                            }
                        for (int m=0;m<3;m++)
                            if (verts[R][0]!=ii){
                                int temp1=verts[R][0];
                                int temp2=tris[R][0];
                                verts[R][0]=verts[R][1];
                                verts[R][1]=verts[R][2];
                                verts[R][2]=temp1;
                                tris[R][0]=tris[R][1];
                                tris[R][1]=tris[R][2];
                                tris[R][2]=temp2;
                            }
                        
                        // fix vertices and adjacency of L
                        verts[L][2]=oppVert;
                        for (int m=0;m<3;m++)
                            if (tris[L][m]==C){
                                tris[L][m]=R;
                                break;
                            }
                        for (int m=0;m<3;m++)
                            if (tris[L][m]==R){
                                tris[L][m]=A;
                                break;
                            }
                        // add L and R to stack if they have triangles opposite P;
                        if (tris[L][1]>=0)
                            triangleStack[++tos]=L;
                        if (tris[R][1]>=0)
                            triangleStack[++tos]=R;
                    }
                }
                break;
            }
        
            // adjust j in the direction of target point ii
            float AB[2]={points[verts[j][1]][0]-points[verts[j][0]][0],points[verts[j][1]][1]-points[verts[j][0]][1]};
            float BC[2]={points[verts[j][2]][0]-points[verts[j][1]][0],points[verts[j][2]][1]-points[verts[j][1]][1]};
            float CA[2]={points[verts[j][0]][0]-points[verts[j][2]][0],points[verts[j][0]][1]-points[verts[j][2]][1]};
            float AP[2]={points[ii][0]-points[verts[j][0]][0],points[ii][1]-points[verts[j][0]][1]};
            float BP[2]={points[ii][0]-points[verts[j][1]][0],points[ii][1]-points[verts[j][1]][1]};
            float CP[2]={points[ii][0]-points[verts[j][2]][0],points[ii][1]-points[verts[j][2]][1]};
            float N1[2]={AB[1],-AB[0]};
            float N2[2]={BC[1],-BC[0]};
            float N3[2]={CA[1],-CA[0]};
            float S1=AP[0]*N1[0]+AP[1]*N1[1];
            float S2=BP[0]*N2[0]+BP[1]*N2[1];
            float S3=CP[0]*N3[0]+CP[1]*N3[1];
            if ((S1>0)&&(S1>=S2)&&(S1>=S3))
                j=tris[j][0];
            else if ((S2>0)&&(S2>=S1)&&(S2>=S3))
                j=tris[j][1];
            else if ((S3>0)&&(S3>=S1)&&(S3>=S2))
                j=tris[j][2];
        }
    }
    
    // count how many triangles we have that dont involve supertriangle vertices
    int nT_final=nT;
    int *renumberAdj=(int *)PushSize(Arena, nT*sizeof(int));
    bool *deadTris=(bool *)PushSize(Arena, nT*sizeof(bool));
    for (int i=0;i<nT;i++)      
        if ((verts[i][0]>=(numPoints-3))
            ||(verts[i][1]>=(numPoints-3))
            ||(verts[i][2]>=(numPoints-3))){
            deadTris[i]=1;
            renumberAdj[i]=nT-(nT_final--);
        }
        else 
            renumberAdj[i]=nT-(nT_final);

    // delete any triangles that contain the supertriangle vertices
    int (*verts_final)[3]=(int (*)[3])PushSize(Arena,3*nT_final*sizeof(int));
    int (*tris_final)[3]=(int (*)[3])PushSize(Arena,3*nT_final*sizeof(int));
    numPoints-=3;
    int index=0;
    for (int i=0;i<nT;i++)      
        if ((verts[i][0]<(numPoints))
            &&(verts[i][1]<(numPoints))
            &&(verts[i][2]<(numPoints))){
            verts_final[index][0]=verts[i][0];
            verts_final[index][1]=verts[i][1];
            verts_final[index][2]=verts[i][2];
            tris_final[index][0]=(1-deadTris[tris[i][0]])*tris[i][0]-deadTris[tris[i][0]];
            tris_final[index][1]=(1-deadTris[tris[i][1]])*tris[i][1]-deadTris[tris[i][1]];
            tris_final[index++][2]=(1-deadTris[tris[i][2]])*tris[i][2]-deadTris[tris[i][2]];
        }
    for (int i=0;i<nT_final;i++){
        if (tris_final[i][0]>=0)
            tris_final[i][0]-=renumberAdj[tris_final[i][0]];
        if (tris_final[i][1]>=0)
            tris_final[i][1]-=renumberAdj[tris_final[i][1]];
        if (tris_final[i][2]>=0)
            tris_final[i][2]-=renumberAdj[tris_final[i][2]];
    }

    printf("Post-Triangulation, Pre-Constraint Vertices:\n");
    for (int i=0;i<nT_final;i++)
        printf("[%d,%d,%d]\n",verts_final[i][0],verts_final[i][1],verts_final[i][2]);
    
    // start video 017

    struct ARRAYLIST* iter=constraintEdges;
    while (iter!=NULL){ // loop thru all constraint edge loops
        for (int i=0;i<(iter->numVals-1);i++){ // loop thru edge in the constraint edge loop
            int vert1 = 0;
            int vert2 = 0;
            for (int j=0;j<numPoints;j++){ 
                if (pointOrder[j]==iter->array[i])
                    vert1=j;
                if (pointOrder[j]==iter->array[i+1])
                    vert2=j;
            }
            
            // generate a list of triangles that each vertex is contained in, if we have a lot of constraint edges, it would be good to pull this out of the loop
            int *vert1_tris = (int *)PushSize(Arena,sizeof(int)*(numPoints+3)); // should be long enough
            int *vert1_tris_ind = (int *)PushSize(Arena,sizeof(int)*(numPoints+3)); // should be long enough
            int *vert2_tris = (int *)PushSize(Arena,sizeof(int)*(numPoints+3)); // should be long enough
            int ind1=0,ind2=0;
            for (int j=0;j<nT_final;j++){ // loop thru all triangles to see which triangles contains our important vertices
                if (verts_final[j][0]==vert1){
                    vert1_tris_ind[ind1]=0;
                    vert1_tris[ind1++]=j;
                }
                else if (verts_final[j][1]==vert1){
                    vert1_tris_ind[ind1]=1;
                    vert1_tris[ind1++]=j;
                }
                else if (verts_final[j][2]==vert1){
                    vert1_tris_ind[ind1]=2;
                    vert1_tris[ind1++]=j;
                }
                if (verts_final[j][0]==vert2||verts_final[j][1]==vert2||verts_final[j][2]==vert2)
                    vert2_tris[ind2++]=j;
            }

            bool edgeFound=0;
            int triTraj=-1;
            int triVertexInd=-1;
            for (int j=0;j<ind1;j++){ // look thru all triangles containing vert1, looking for vert2, or until you find an edge that intersects vert1-vert2
                if (verts_final[vert1_tris[j]][0]==vert2||verts_final[vert1_tris[j]][1]==vert2||verts_final[vert1_tris[j]][2]==vert2){ // exact edge found
                    edgeFound=1;
                    break;
                }
                else if (pointDirectionFromLineSegment2D(points[vert1], // is vert2 to the left of both edges adjacent to vert1 on triangle j
                        points[verts_final[vert1_tris[j]][(vert1_tris_ind[j]+1)%3]],
                        points[vert2])&&
                        pointDirectionFromLineSegment2D(points[verts_final[vert1_tris[j]][(vert1_tris_ind[j]+2)%3]],
                        points[vert1],
                        points[vert2])){
                    triTraj=vert1_tris[j];
                    triVertexInd=vert1_tris_ind[j];
                    break;
                }
            }
            if (edgeFound){ // constraint edge already in vertex array, do nothing
            
            }
            else { // constraint edge NOT found in vertex array
                // start looking for intersections in triangle triTraj
                struct ARRAYLIST* intersections=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                struct ARRAYLIST* iter2=intersections;
                iter2->next=NULL; // add the first intersection edge details
                iter2->numVals=-1;

                while (1){
                    if ((verts_final[triTraj][0]==vert2)|| // we made it to the triangle containing vert2
                        (verts_final[triTraj][1]==vert2)||
                        (verts_final[triTraj][2]==vert2)){
                        break;
                    }
                    for (int j=0;j<3;j++){ // check if vert2 is to the right of any edge in the current triangle

                        if (!pointDirectionFromLineSegment2D(points[verts_final[triTraj][j]], // if it is, it's a candidate for intersection
                            points[verts_final[triTraj][(j+1)%3]], // because the entry edge will fail this test, so no double counting
                            points[vert2])){

                            if (lineSegmentsCross2D(points[vert1],points[vert2],points[verts_final[triTraj][j]],points[verts_final[triTraj][(j+1)%3]])){
                                if (iter2->numVals==-1){
                                    iter2->next=NULL;
                                    iter2->numVals=2;
                                    iter2->array[0]=verts_final[triTraj][j];
                                    iter2->array[1]=verts_final[triTraj][(j+1)%3];
                                }
                                else{
                                    struct ARRAYLIST* nextIntersection=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                                    iter2->next=nextIntersection;
                                    iter2=iter2->next;
                                    iter2->next=NULL;
                                    iter2->numVals=2;
                                    iter2->array[0]=verts_final[triTraj][j];
                                    iter2->array[1]=verts_final[triTraj][(j+1)%3];
                                }
                                    triTraj=tris_final[triTraj][j];
                                break;
                            }
                        }
                    }
                }
                // loop thru edges that need to be removed, can't save the triangle index because we are killing triangles :(
                struct ARRAYLIST* iter3=intersections;
                struct ARRAYLIST* iter99=intersections;
        
                int triangle0=-1, triangle1=-1;
                // new edges that we are adding to the triangulation
                struct ARRAYLIST* newEdges=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                struct ARRAYLIST* iter4=newEdges;
                newEdges->numVals=-1;
                while (iter3!=NULL){
                    // find triangles that contain this edge (one forward, one backward)
                    int triangle0_other_ind=-1;
                    int triangle1_other_ind=-1;
                    for (int j=0;j<nT_final;j++){
                        if ((verts_final[j][0]==iter3->array[1])&&(verts_final[j][1]==iter3->array[0])){
                            triangle1=j;
                            triangle1_other_ind=2;
                            break;
                        }                           
                        if ((verts_final[j][1]==iter3->array[1])&&(verts_final[j][2]==iter3->array[0])){
                            triangle1=j;
                            triangle1_other_ind=0;
                            break;
                        }                           
                        if ((verts_final[j][2]==iter3->array[1])&&(verts_final[j][0]==iter3->array[0])){
                            triangle1=j;
                            triangle1_other_ind=1;
                            break;
                        }                           
                    }
                    for (int j=0;j<nT_final;j++){
                        if ((verts_final[j][0]==iter3->array[0])&&(verts_final[j][1]==iter3->array[1])){
                            triangle0=j;
                            triangle0_other_ind=2;
                            break;
                        }                           
                        if ((verts_final[j][1]==iter3->array[0])&&(verts_final[j][2]==iter3->array[1])){
                            triangle0=j;
                            triangle0_other_ind=0;
                            break;
                        }                           
                        if ((verts_final[j][2]==iter3->array[0])&&(verts_final[j][0]==iter3->array[1])){
                            triangle0=j;
                            triangle0_other_ind=1;
                            break;
                        }                           
                    }
                    
                    if (quadrilateralConvex2D(points[verts_final[triangle0][(triangle0_other_ind+2)%3]],
                                            points[verts_final[triangle0][triangle0_other_ind]],
                                            points[verts_final[triangle0][(triangle0_other_ind+1)%3]],
                                            points[verts_final[triangle1][triangle1_other_ind]])){

                        // triangle convex, swap diagonal
                        verts_final[triangle0][(triangle0_other_ind+2)%3]=verts_final[triangle1][triangle1_other_ind];
                        verts_final[triangle1][(triangle1_other_ind+2)%3]=verts_final[triangle0][triangle0_other_ind];

                        // fix primary triangle adjacencies
                        tris_final[triangle0][(triangle0_other_ind+1)%3]=tris_final[triangle1][(triangle1_other_ind+2)%3];
                        tris_final[triangle1][(triangle1_other_ind+1)%3]=tris_final[triangle0][(triangle0_other_ind+2)%3];


                        tris_final[triangle0][(triangle0_other_ind+2)%3]=triangle1;
                        tris_final[triangle1][(triangle1_other_ind+2)%3]=triangle0;
                        // fix 2 nearby triangle adjacencies
                        int tri2Fix0=tris_final[triangle0][triangle0_other_ind];
                        int tri2Fix1=tris_final[triangle1][triangle1_other_ind];
                        for (int m=0;m<3;m++)
                            if (tris_final[tri2Fix0][m]==triangle0){
                                tris_final[tri2Fix0][m]=triangle1;
                                break;
                            }
                        for (int m=0;m<3;m++)
                            if (tris_final[tri2Fix1][m]==triangle1){
                                tris_final[tri2Fix1][m]=triangle0;
                                break;
                            }

                        triangle0_other_ind++; // rotate around 1 vtx
                        triangle1_other_ind++; // rotate around 1 vtx



                        // if the new diagonal still intersects the constraint edge, add it to the list
                        if (lineSegmentsCross2D(points[vert1],points[vert2],
                                                points[verts_final[triangle0][(triangle0_other_ind+1)%3]],
                                                points[verts_final[triangle1][(triangle1_other_ind+1)%3]])){
                            struct ARRAYLIST* nextIntersection=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                            iter2->next=nextIntersection;
                            iter2=iter2->next;
                            iter2->next=NULL;
                            iter2->numVals=2;
                            iter2->array[0]=verts_final[triangle0][(triangle0_other_ind+1)%3];
                            iter2->array[1]=verts_final[triangle1][(triangle1_other_ind+1)%3];
                        }
                        else {
                            // if diagonal doesnt intersect the constraint edge, add to newEdge list
                            if (iter4->numVals==-1){ // first new edge
                                iter4->next=NULL;
                                iter4->numVals=2; // 2 vertices 
                                iter4->array[0]=verts_final[triangle0][(triangle0_other_ind+1)%3];
                                iter4->array[1]=verts_final[triangle1][(triangle1_other_ind+1)%3];
                            }
                            else {
                                struct ARRAYLIST* nextEdge=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                                iter4->next=nextEdge;
                                iter4=iter4->next;
                                iter4->next=NULL;
                                iter4->numVals=2; // 2 vertices 
                                iter4->array[0]=verts_final[triangle0][(triangle0_other_ind+1)%3];
                                iter4->array[1]=verts_final[triangle1][(triangle1_other_ind+1)%3];                          
                            }
                        }
                    }
                    else {
                        // quad not convex, put diagonal on the end of the linked list
                        struct ARRAYLIST* iter5=intersections;
                        struct ARRAYLIST* prev5=NULL;
                        while (iter5->next!=NULL){
                            if (iter5==iter3){ // remove the old diagonal from the linked list
                                if (prev5==NULL){
                                    intersections=iter3->next; // free the skipped one? (leak here?)
                                }
                                else {
                                    prev5->next=iter3->next; // again free the skipped one? (leak here?)
                                }
                            }
                            prev5=iter5;
                            iter5=iter5->next;
                        }
                        struct ARRAYLIST* nextIntersection=(struct ARRAYLIST*)PushSize(Arena, (2+1)*sizeof(int)+sizeof(struct ARRAYLIST*));
                        iter5->next=nextIntersection;
                        iter5=iter5->next;
                        iter5->next=NULL;
                        iter5->numVals=2;
                        iter5->array[0]=verts_final[triangle0][(triangle0_other_ind+1)%3];
                        iter5->array[1]=verts_final[triangle1][(triangle1_other_ind+1)%3];

                    }
                iter3=iter3->next;
                }
            
                int swaps=1;
                while (swaps>0){ // until no further swaps take place
                    swaps=0;
                    // restore the delaunay triangulation on the newEdges
                    // loop over newEdges
                    struct ARRAYLIST* iter6=newEdges;
                    while (iter6!=NULL){

                        // if the new edge is NOT the constraint edge
                        if (!(((iter6->array[0]==vert1)&&
                            (iter6->array[1]==vert2))||
                            ((iter6->array[0]==vert2)&&
                            (iter6->array[1]==vert1)))){
                            // identify lone vertex indices for the triangles which share the new edge, can keep an edge adjacency array to speed this up
                            
                            // find triangles that contain this edge (one forward, one backward)
                            for (int j=0;j<nT_final;j++){
                                if (((verts_final[j][0]==iter6->array[1])&&(verts_final[j][1]==iter6->array[0]))||
                                    ((verts_final[j][1]==iter6->array[1])&&(verts_final[j][2]==iter6->array[0]))||
                                    ((verts_final[j][2]==iter6->array[1])&&(verts_final[j][0]==iter6->array[0]))){
                                    triangle0=j;
                                    break;
                                }
                            }
                            for (int j=0;j<nT_final;j++){
                                if (((verts_final[j][0]==iter6->array[0])&&(verts_final[j][1]==iter6->array[1]))||
                                    ((verts_final[j][1]==iter6->array[0])&&(verts_final[j][2]==iter6->array[1]))||
                                    ((verts_final[j][2]==iter6->array[0])&&(verts_final[j][0]==iter6->array[1]))){
                                    triangle1=j;
                                    break;
                                }
                            }

                            int triangle0_lone_vtx_ind=-1;
                            int triangle1_lone_vtx_ind=-1;

                            for (int j=0;j<3;j++) // find the lone vertex in triangle0
                                if ((verts_final[triangle0][j]!=iter6->array[0])&&
                                   (verts_final[triangle0][j]!=iter6->array[1])){
                                    triangle0_lone_vtx_ind=j;
                                }
                            for (int j=0;j<3;j++) // find the lone vertex in triangle1
                                if ((verts_final[triangle1][j]!=iter6->array[0])&&
                                   (verts_final[triangle1][j]!=iter6->array[1])){
                                    triangle1_lone_vtx_ind=j;
                                }

                            // if delaunay condition is not satisfied for triangles that share this edge, swap the diagonal
                            float v1a[2]={points[verts_final[triangle0][(triangle0_lone_vtx_ind)]][0],
                                     points[verts_final[triangle0][(triangle0_lone_vtx_ind)]][1]};
                            float v1b[2]={points[verts_final[triangle0][(triangle0_lone_vtx_ind+1)%3]][0],
                                     points[verts_final[triangle0][(triangle0_lone_vtx_ind+1)%3]][1]};
                            float v1c[2]={points[verts_final[triangle0][(triangle0_lone_vtx_ind+2)%3]][0],
                                     points[verts_final[triangle0][(triangle0_lone_vtx_ind+2)%3]][1]};  
                            float v2a[2]={points[verts_final[triangle1][(triangle1_lone_vtx_ind)]][0],
                                     points[verts_final[triangle1][(triangle1_lone_vtx_ind)]][1]};
                            float cosa1=((v1b[0]-v1a[0])*(v1c[0]-v1a[0])+(v1b[1]-v1a[1])*(v1c[1]-v1a[1]));
                            float cosa2=((v1c[0]-v2a[0])*(v1b[0]-v2a[0])+(v1c[1]-v2a[1])*(v1b[1]-v2a[1]));
                            float cosb1=((v1c[0]-v2a[0])*(v1b[0]-v2a[0])+(v1c[1]-v2a[1])*(v1b[1]-v2a[1]));
                            float cosb2=((v1b[0]-v1a[0])*(v1c[0]-v1a[0])+(v1b[1]-v1a[1])*(v1c[1]-v1a[1]));
                            if (((((cosa1<0)&&(cosb1<0))|| // if triangle 0 vertex is inside triangle 1 circumcircle
                                ((-cosa1*((v1c[0]-v2a[0])*(v1b[1]-v2a[1])-(v1c[1]-v2a[1])*(v1b[0]-v2a[0])))>
                                 (cosb1*((v1b[0]-v1a[0])*(v1c[1]-v1a[1])-(v1b[1]-v1a[1])*(v1c[0]-v1a[0]))))))|| // or
                                 ((((cosa2<0)&&(cosb2<0))|| // if triangle 1 vertex is inside triangle 0 circumcircle
                                ((-cosa2*((v1b[0]-v1a[0])*(v1c[1]-v1a[1])-(v1b[1]-v1a[1])*(v1c[0]-v1a[0])))>
                                 (cosb2*((v1c[0]-v2a[0])*(v1b[1]-v2a[1])-(v1c[1]-v2a[1])*(v1b[0]-v2a[0]))))))){

                                verts_final[triangle0][(triangle0_lone_vtx_ind+2)%3]=verts_final[triangle1][triangle1_lone_vtx_ind];
                                verts_final[triangle1][(triangle1_lone_vtx_ind+2)%3]=verts_final[triangle0][triangle0_lone_vtx_ind];

                                // update triangle0 and triangle1 adjacencies
                                tris_final[triangle0][(triangle0_lone_vtx_ind+1)%3]=tris_final[triangle1][(triangle1_lone_vtx_ind+2)%3];
                                tris_final[triangle1][(triangle1_lone_vtx_ind+1)%3]=tris_final[triangle0][(triangle0_lone_vtx_ind+2)%3];
                                tris_final[triangle0][(triangle0_lone_vtx_ind+2)%3]=triangle1;
                                tris_final[triangle1][(triangle1_lone_vtx_ind+2)%3]=triangle0;
                                // fix 2 nearby triangle adjacencies
                                int tri2Fix0=tris_final[triangle0][triangle0_lone_vtx_ind];
                                int tri2Fix1=tris_final[triangle1][triangle1_lone_vtx_ind];
                                for (int m=0;m<3;m++)
                                    if (tris_final[tri2Fix0][m]==triangle0){
                                        tris_final[tri2Fix0][m]=triangle1;
                                        break;
                                    }
                                for (int m=0;m<3;m++)
                                    if (tris_final[tri2Fix1][m]==triangle1){
                                        tris_final[tri2Fix1][m]=triangle0;
                                        break;
                                    }

                                iter6->array[0]=verts_final[triangle0][triangle0_lone_vtx_ind]; // swap diagonal in new edge list
                                iter6->array[1]=verts_final[triangle1][triangle1_lone_vtx_ind]; // swap diagonal in new edge list

                                triangle0_lone_vtx_ind++; // rotate the lone vtx +1
                                triangle1_lone_vtx_ind++; // rotate the lone vtx +1

                                swaps++; // keep track of number of swaps

                            }
                        }
                        iter6=iter6->next;
                    }
                }
            }
        }
        iter=iter->next;
    }

    // end video 017

    // undo the mapping
    for (int i=0;i<numPoints;i++){
        points[i][0]=points[i][0]*d+xmin;
        points[i][1]=points[i][1]*d+ymin;
    }

    for(s32 i = 0; i < nT_final; i++)
    {
        triangle T = {};
        T.Vertices[0] = V2(points[verts_final[i][0]][0], points[verts_final[i][0]][1]);
        T.Vertices[1] = V2(points[verts_final[i][1]][0], points[verts_final[i][1]][1]);
        T.Vertices[2] = V2(points[verts_final[i][2]][0], points[verts_final[i][2]][1]);

        PushTriangle(RenderGroup, Flat, T, 5.0f, V4(DebugColorTable[i], 0.7f));
    }
    
    printf("\nPost-Constraint Vertices:\n");
    for (int i=0;i<nT_final;i++)
        printf("[%d,%d,%d]\n",verts_final[i][0],verts_final[i][1],verts_final[i][2]);
    printf("\nPoint Cloud:\n");
    for (int i=0;i<numPoints;i++)
        printf("%d: (%f,%f)\n",i,points[i][0],points[i][1]);

    return;
}
