#if !defined(EDITOR_DEBUG_UI_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct debug_state;

enum debug_text_op
{
    DEBUGTextOp_DrawText,
    DEBUGTextOp_SizeText,
};

enum debug_interaction_type
{
    DebugInteraction_None,

    DebugInteraction_NOP,

    DebugInteraction_AutoModifyVariable,

    DebugInteraction_ToggleValue,
    DebugInteraction_DragValue,
    DebugInteraction_TearValue,

    DebugInteraction_Resize,
    DebugInteraction_Move,

    DebugInteraction_Select,

    DebugInteraction_ToggleExpansion,

    DebugInteraction_SetUInt32,
    DebugInteraction_SetPointer,
};

struct debug_interaction
{
    debug_id ID;
    debug_interaction_type Type;

    void *Target;
    union
    {
        void *Generic;
        void *Pointer;
        u32 UInt32;
        debug_tree *Tree;
        debug_variable_link *Link;
        debug_type DebugType;
        v2 *P;
        debug_element *Element;
    };
};

struct layout
{
    debug_state *DebugState;
    v2 MouseP;
    v2 BaseCorner;

    u32 Depth;

    v2 At;
    r32 LineAdvance;
    r32 NextYDelta;
    r32 SpacingX;
    r32 SpacingY;
    
    u32 NoLineFeed;
    b32 LineInitialized;
};

struct layout_element
{
    // NOTE(casey): Storage;
    layout *Layout;
    v2 *Dim;
    v2 *Size;
    debug_interaction Interaction;

    // NOTE(casey): Out
    rectangle2 Bounds;
};

#define EDITOR_DEBUG_UI_H
#endif
