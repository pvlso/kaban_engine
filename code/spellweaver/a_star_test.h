#if !defined(A_STAR_TEST_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct as_node
{
    b32 Obstacle;
    b32 Visited;

    r32 GlobalGoal;
    r32 LocalGoal;

    s32 X;
    s32 Y;

    as_node *Neighbours[8];
    as_node *Parent;
};

struct game_mode_a_star_test
{
    u32 NodeCount;
    as_node *Nodes;

    as_node *StartNode;
    as_node *EndNode;

    heap Heap;
    
    u32 NotTestedCount;
    sort_entry NotTestedNodes[4096];
    sort_entry Temp[4096];
};


#define A_STAR_TEST_H
#endif
