/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_render_group.h"

struct opengl_info
{
    b32 ModernContext;

    char *Vendor;
    char *Renderer;
    char *Version;
    char *ShadingLanguageVersion;
    char *Extensions;
};

internal opengl_info
OpenGLGetInfo(b32 ModernContext)
{
    opengl_info Result = {};

    Result.ModernContext = ModernContext;
    Result.Vendor = (char *)glGetString(GL_VENDOR);
    Result.Renderer = (char *)glGetString(GL_RENDERER);
    Result.Version = (char *)glGetString(GL_VERSION);
    if(Result.ModernContext)
    {
        Result.ShadingLanguageVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    }
    else
    {
        Result.ShadingLanguageVersion = "(none)";
    }

    Result.Extensions = (char *)glGetString(GL_EXTENSIONS);

    char *MajorAt = Result.Version;
    char *MinorAt = 0;
    for(char *At = Result.Version;
        *At;
        ++At)
    {
        if(At[0] == '.')
        {
            MinorAt = At + 1;
            break;
        }
    }

    s32 Major = 1;
    s32 Minor = 0;
    if(MinorAt)
    {
        Major = S32FromZ(MajorAt);
        Minor = S32FromZ(MinorAt);
    }
    
    return(Result);
}

internal opengl_info
OpenGLInit(b32 ModernContext, b32 FramebufferSupportsSRGB)
{
    opengl_info Info = OpenGLGetInfo(ModernContext);

    // NOTE(casey): If we believe we can do full sRGB on the texture side
    // and the framebuffer side, then we can enable it, otherwise it is
    // safer for us to pass it straight through.
    OpenGLDefaultInternalTextureFormat = GL_RGBA8;
    if(FramebufferSupportsSRGB && GLEW_EXT_texture_sRGB && GLEW_EXT_framebuffer_sRGB)
    {
        OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;

        glEnable(GL_FRAMEBUFFER_SRGB);
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    return(Info);
}       

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
    glTexImage2D(GL_TEXTURE_2D, 0, OpenGLDefaultInternalTextureFormat, 
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

    glTexImage2D(GL_TEXTURE_2D, 0, OpenGLDefaultInternalTextureFormat, Width, Height, 0,
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


internal void
NkOpenGLUploadAtlas(nk_opengl *Ogl, const void *image, int width, int height)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    glGenTextures(1, &Ogl->font_tex);
    glBindTexture(GL_TEXTURE_2D, Ogl->font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    
    glTexImage2D(GL_TEXTURE_2D, 0, OpenGLDefaultInternalTextureFormat, (GLsizei)width, (GLsizei)height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, image);
}

struct nk_gl_vertex
{
    float position[2];
    float uv[2];
    float col[4];
};

internal void
NKOpenGLRenderCommands(nk_win32 *NkWin32, rectangle2i DrawRegion, enum nk_anti_aliasing AA)
{
    TIMED_FUNCTION();
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    /* setup global state */
    struct nk_opengl *dev = &NkWin32->ogl;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_TRANSFORM_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* setup viewport/project */
//    glViewport(0,0,(GLsizei)NkWin32->display_width,(GLsizei)NkWin32->display_height);
    glViewport(DrawRegion.MinX, DrawRegion.MinY, (GLsizei)NkWin32->display_width,(GLsizei)NkWin32->display_height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, NkWin32->width, NkWin32->height, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    {
        GLsizei vs = sizeof(struct nk_gl_vertex);
        size_t vp = offsetof(struct nk_gl_vertex, position);
        size_t vt = offsetof(struct nk_gl_vertex, uv);
        size_t vc = offsetof(struct nk_gl_vertex, col);

        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* fill convert configuration */
        struct nk_convert_config config;
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_gl_vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_gl_vertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(struct nk_gl_vertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };

        memset(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(struct nk_gl_vertex);
        config.vertex_alignment = NK_ALIGNOF(struct nk_gl_vertex);
        config.tex_null = dev->tex_null;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;
        config.global_alpha = 1.0f;
        config.shape_AA = AA;
        config.line_AA = AA;

        /* convert shapes into vertexes */
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ebuf);
        nk_convert(&NkWin32->ctx, &dev->cmds, &vbuf, &ebuf, &config);

        /* setup vertex buffer pointer */
        {const void *vertices = nk_buffer_memory_const(&vbuf);
            glVertexPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vp));
            glTexCoordPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vt));
            glColorPointer(4, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vc));}

        /* iterate over and execute each draw command */
        offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);
        nk_draw_foreach(cmd, &NkWin32->ctx, &dev->cmds)
        {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor(
                (GLint)(cmd->clip_rect.x * NkWin32->fb_scale.x),
                (GLint)((NkWin32->height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * NkWin32->fb_scale.y),
                (GLint)(cmd->clip_rect.w * NkWin32->fb_scale.x),
                (GLint)(cmd->clip_rect.h * NkWin32->fb_scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(&NkWin32->ctx);
        nk_buffer_clear(&dev->cmds);
        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }

    /* default OpenGL state */
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}
