#if !defined(EDITOR_RENDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct texture_op_allocate
{
    u32 Width;
    u32 Height;
    void *Data;

    void **ResultHandle;
};

struct texture_op_deallocate
{
    void *Handle;
};

struct texture_op
{
    texture_op *Next;
    b32 IsAllocate;
    union
    {
        texture_op_allocate Allocate;
        texture_op_deallocate Deallocate;
    };
    
};

#define EDITOR_RENDER_H
#endif
