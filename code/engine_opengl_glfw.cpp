/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_render_group.h"

#define GL_MAJOR_VERSION                                   0x821B
#define GL_MINOR_VERSION                                   0x821C

#define GL_FRAMEBUFFER_SRGB                                0x8DB9
#define GL_SRGB8_ALPHA8                                    0x8C43
#define GL_SRGB8                                           0x8C41
#define GL_SRGB_ALPHA                                      0x8C42
#define GL_SHADING_LANGUAGE_VERSION                        0x8B8C

#define GL_CLAMP_TO_EDGE                                   0x812F

#define GL_FRAMEBUFFER                                     0x8D40
#define GL_COLOR_ATTACHMENT0                               0x8CE0
#define GL_FRAMEBUFFER_COMPLETE                            0x8CD5

// NOTE(casey): Windows-specific
#define WGL_CONTEXT_MAJOR_VERSION_ARB                      0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB                      0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB                        0x2093
#define WGL_CONTEXT_FLAGS_ARB                              0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB                       0x9126

#define WGL_CONTEXT_DEBUG_BIT_ARB                          0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB             0x0002

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB                   0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB          0x00000002

inline void
OpenGLSetScreenspace(s32 Width, s32 Height)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    r32 a = SafeRatio1(2.0f, (r32)Width);
    r32 b = SafeRatio1(2.0f, (r32)Height);
    r32 Proj[] =
    {
         a,  0,  0,  0,
         0,  b,  0,  0,
         0,  0,  1,  0,
        -1, -1,  0,  1,
    };
    glLoadMatrixf(Proj);
}

internal void *
OpenGLAllocateTexture(u32 Width, u32 Height, void *Data)
{
    GLuint Handle;
    glGenTextures(1, &Handle);
    glBindTexture(GL_TEXTURE_2D, Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 
                 Width, Height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, Data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    

    glBindTexture(GL_TEXTURE_2D, 0);

    Assert(sizeof(Handle) <= sizeof(void *));
    void *Result = PointerFromU32(void, Handle);

    return(Result);
}

inline void
OpenGLLine(v2 P0, v2 P1, v4 Color)
{
    glBegin(GL_LINES);

    glColor4f(Color.r, Color.g, Color.b, Color.a);

    glVertex2f(P0.x, P0.y);
    glVertex2f(P1.x, P1.y);

    glEnd();
}

inline void
OpenGLTriangle(v2 a, v2 b, v2 c, v4 Color)
{                    
    glBegin(GL_TRIANGLES);

    glColor4f(Color.r, Color.g, Color.b, Color.a);

    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glVertex2f(c.x, c.y);

    glEnd();
}

inline void
OpenGLRectangle(v2 MinP, v2 MaxP, v4 Color, v2 MinUV = V2(0, 0), v2 MaxUV = V2(1, 1))
{                    
    glBegin(GL_TRIANGLES);

    glColor4f(Color.r, Color.g, Color.b, Color.a);

    // NOTE(casey): Lower triangle
    glTexCoord2f(MinUV.x, MinUV.y);
    glVertex2f(MinP.x, MinP.y);

    glTexCoord2f(MaxUV.x, MinUV.y);
    glVertex2f(MaxP.x, MinP.y);

    glTexCoord2f(MaxUV.x, MaxUV.y);
    glVertex2f(MaxP.x, MaxP.y);

    // NOTE(casey): Upper triangle
    glTexCoord2f(MinUV.x, MinUV.y);
    glVertex2f(MinP.x, MinP.y);

    glTexCoord2f(MaxUV.x, MaxUV.y);
    glVertex2f(MaxP.x, MaxP.y);

    glTexCoord2f(MinUV.x, MaxUV.y);
    glVertex2f(MinP.x, MaxP.y);

    glEnd();
}

global_variable u32 GlobalFramebufferCount = 1;
global_variable GLuint GlobalFramebufferHandles[256] = {0};
global_variable GLuint GlobalFramebufferTextures[256] = {0};

internal void
OpenGLBindFramebuffer(u32 TargetIndex, rectangle2i DrawRegion)
{
    s32 WindowWidth = GetWidth(DrawRegion);
    s32 WindowHeight = GetHeight(DrawRegion);

    glBindFramebuffer(GL_FRAMEBUFFER, GlobalFramebufferHandles[TargetIndex]);
    if(TargetIndex > 0)
    {
        glViewport(0, 0, WindowWidth, WindowHeight);
    }
    else
    {
        glViewport(DrawRegion.MinX, DrawRegion.MinY, WindowWidth, WindowHeight);
    }
}

inline void
OpenGLDisplayBitmap(s32 Width, s32 Height, void *Memory, int Pitch,
                    rectangle2i DrawRegion, v4 ClearColor, GLuint BlitTexture)
{
    Assert(Pitch == Width*4);
    OpenGLBindFramebuffer(0, DrawRegion);
    
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, BlitTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, Width, Height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, Memory);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    

    glEnable(GL_TEXTURE_2D);

    glClearColor(ClearColor.r, ClearColor.g, ClearColor.b, ClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    OpenGLSetScreenspace(Width, Height);

    // TODO(casey): Decide how we want to handle aspect ratio - black bars or crop?

    v2 MinP = {0, 0};
    v2 MaxP = {(r32)Width, (r32)Height};
    v4 Color = {1, 1, 1, 1};

    OpenGLRectangle(MinP, MaxP, Color);

    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_BLEND);
}

inline void
OpenGLLineVertices(v2 MinP, v2 MaxP)
{                    
    glVertex2f(MinP.x, MinP.y);
    glVertex2f(MaxP.x, MinP.y);

    glVertex2f(MaxP.x, MinP.y);
    glVertex2f(MaxP.x, MaxP.y);

    glVertex2f(MaxP.x, MaxP.y);
    glVertex2f(MinP.x, MaxP.y);

    glVertex2f(MinP.x, MaxP.y);
    glVertex2f(MinP.x, MinP.y);
}

internal void
OpenGLRenderCommands(editor_render_commands *Commands, editor_render_prep *Prep, rectangle2i DrawRegion,
                     u32 WindowWidth, u32 WindowHeight)
{    
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    b32 UseRenderTargets = (glBindFramebuffer != 0);

    u32 MaxRenderTargetIndex = UseRenderTargets ? Commands->MaxRenderTargetIndex : 0;
     if(MaxRenderTargetIndex >= GlobalFramebufferCount)
    {
        u32 NewFramebufferCount = Commands->MaxRenderTargetIndex + 1;
        Assert(NewFramebufferCount < ArrayCount(GlobalFramebufferHandles));
        u32 NewCount = NewFramebufferCount - GlobalFramebufferCount;
        glGenFramebuffers(NewCount, GlobalFramebufferHandles + GlobalFramebufferCount);
        glGenTextures(NewCount, GlobalFramebufferTextures + GlobalFramebufferCount);
        
        for(u32 TargetIndex = GlobalFramebufferCount;
            TargetIndex <= NewFramebufferCount;
            ++TargetIndex)
        {
            GLuint TextureHandle = U32FromPointer(OpenGLAllocateTexture(GetWidth(DrawRegion), GetHeight(DrawRegion), 0));
            GlobalFramebufferTextures[TargetIndex] = TextureHandle;
            glBindFramebuffer(GL_FRAMEBUFFER, GlobalFramebufferHandles[TargetIndex]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TextureHandle, 0);

            GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            Assert(Status == GL_FRAMEBUFFER_COMPLETE);
        }

        GlobalFramebufferCount = NewFramebufferCount;
    }
    
    for(u32 TargetIndex = 0;
        TargetIndex <= MaxRenderTargetIndex;
        ++TargetIndex)
    {
        if(UseRenderTargets)
        {
            OpenGLBindFramebuffer(TargetIndex, DrawRegion);
        }

        if(TargetIndex == 0)
        {
            glScissor(0, 0, WindowWidth, WindowHeight);
        }
        else
        {
            glScissor(0, 0, GetWidth(DrawRegion), GetHeight(DrawRegion));
        }
        
        glClearColor(Commands->ClearColor.r, Commands->ClearColor.g,
                     Commands->ClearColor.b, Commands->ClearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    OpenGLSetScreenspace(Commands->Width, Commands->Height);

    u32 SortEntryCount = Commands->PushBufferElementCount;
    sort_entry *SortEntries = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);

    u32 CurrentRenderTargetIndex = 0xFFFFFFFF;
    u32 ClipRectIndex = 0xFFFFFFFF;
    sort_entry *SortEntry = SortEntries;
    for(u32 SortEntryIndex = 0;
        SortEntryIndex < SortEntryCount;
        ++SortEntryIndex, ++SortEntry)
    {
        render_group_entry_header *Header = (render_group_entry_header *)
            (Commands->PushBufferBase + SortEntry->Index);
        if(UseRenderTargets || (Prep->ClipRects[Header->ClipRectIndex].RenderTargetIndex <= MaxRenderTargetIndex))
        {
            if(ClipRectIndex != Header->ClipRectIndex)
            {
                ClipRectIndex = Header->ClipRectIndex;
                Assert(ClipRectIndex < Commands->ClipRectCount);
            
                render_entry_cliprect *Clip = Prep->ClipRects + ClipRectIndex;

                rectangle2i ClipRect = Clip->Rect;
                if(CurrentRenderTargetIndex != Clip->RenderTargetIndex)
                {
                    CurrentRenderTargetIndex = Clip->RenderTargetIndex;
                    Assert(CurrentRenderTargetIndex <= MaxRenderTargetIndex);
                    if(UseRenderTargets)
                    {
                        OpenGLBindFramebuffer(CurrentRenderTargetIndex, DrawRegion);
                    }
                }

                if(!UseRenderTargets || (Clip->RenderTargetIndex == 0))
                {
                    ClipRect = Offset(ClipRect, DrawRegion.MinX, DrawRegion.MinY);
                }
            
                glScissor(ClipRect.MinX, ClipRect.MinY, ClipRect.MaxX - ClipRect.MinX, ClipRect.MaxY - ClipRect.MinY);
            }
            
            void *Data = (uint8 *)Header + sizeof(*Header);
            switch(Header->Type)
            {
                case RenderGroupEntryType_render_entry_bitmap:
                {
                    render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
                    Assert(Entry->Bitmap);
                    
                    if(Entry->Bitmap->Width && Entry->Bitmap->Height)
                    {
                        v2 XAxis = {1, 0};
                        v2 YAxis = {0, 1};
                        v2 MinP = Entry->P;
                        v2 MaxP = MinP + Entry->Size.x*XAxis + Entry->Size.y*YAxis;

                        glBindTexture(GL_TEXTURE_2D, (GLuint)U32FromPointer(Entry->Bitmap->TextureHandle));

                        r32 OneTexelU = 0.5f / (r32)Entry->Bitmap->Width;
                        r32 OneTexelV = 0.5f / (r32)Entry->Bitmap->Height;

                        v2 MinUV = V2(OneTexelU, OneTexelV);
                        v2 MaxUV = V2(1.0f - OneTexelU, 1.0f - OneTexelV);

                        OpenGLRectangle(Entry->P, MaxP, Entry->Color);
                    }
                } break;

                case RenderGroupEntryType_render_entry_rectangle:
                {
                    render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                    glDisable(GL_TEXTURE_2D);

                    OpenGLRectangle(Entry->P, Entry->P + Entry->Dim, SRGB1ToLinear1(Entry->Color));
                    glEnable(GL_TEXTURE_2D);
                } break;

                case RenderGroupEntryType_render_entry_line:
                {
                    render_entry_line *Entry = (render_entry_line *)Data;
                    glDisable(GL_TEXTURE_2D);
                    OpenGLLine(Entry->P0, Entry->P1, SRGB1ToLinear1(Entry->Color));
                    glEnable(GL_TEXTURE_2D);
                } break;

                case RenderGroupEntryType_render_entry_triangle:
                {
                    render_entry_triangle *Entry = (render_entry_triangle *)Data;
                    glDisable(GL_TEXTURE_2D);
                    OpenGLTriangle(Entry->A, Entry->B, Entry->C, SRGB1ToLinear1(Entry->Color));
                    glEnable(GL_TEXTURE_2D);
                } break;

                case RenderGroupEntryType_render_entry_blend_render_target:
                {
                    render_entry_blend_render_target *Entry = (render_entry_blend_render_target *)Data;
                    if(UseRenderTargets)
                    {
                        glBindTexture(GL_TEXTURE_2D, GlobalFramebufferTextures[Entry->SourceTargetIndex]);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        OpenGLRectangle(V2(0, 0), V2i(Commands->Width, Commands->Height), V4(1, 1, 1, Entry->Alpha));
                        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    }
                } break;

                InvalidDefaultCase;
            }
        }
    }

    if(UseRenderTargets)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

internal void
OpenGLManageTextures(texture_op *First)
{
    for(texture_op *Op = First;
        Op;
        Op = Op->Next)
    {
        if(Op->IsAllocate)
        {
            *Op->Allocate.ResultHandle = OpenGLAllocateTexture(Op->Allocate.Width, Op->Allocate.Height,
                                                               Op->Allocate.Data);
        }
        else
        {
            GLuint Handle = U32FromPointer(Op->Deallocate.Handle);
            glDeleteTextures(1, &Handle);
        }
    }
}

