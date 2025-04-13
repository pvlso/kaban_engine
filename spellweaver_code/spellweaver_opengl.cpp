/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

#include "spellweaver_render_group.h"


#define GL_FRAMEBUFFER_SRGB               0x8DB9
#define GL_SRGB8_ALPHA8                   0x8C43

#define GL_SHADING_LANGUAGE_VERSION       0x8B8C

// NOTE(casey): Windows-specific
#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126

#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

struct opengl_info
{
    b32 ModernContext;

    char *Vendor;
    char *Renderer;
    char *Version;
    char *ShadingLanguageVersion;
    char *Extensions;

    b32 GL_EXT_texture_sRGB;
    b32 GL_EXT_framebuffer_sRGB;
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

    char *At = Result.Extensions;
    while(*At)
    {
        while(IsWhitespace(*At)) {++At;}
        char *End = At;
        while(*End && !IsWhitespace(*End)) {++End;}

        umm Count = End - At;        

        if(0) {}
        else if(StringsAreEqual(Count, At, "GL_EXT_texture_sRGB")) {Result.GL_EXT_texture_sRGB=true;}
        else if(StringsAreEqual(Count, At, "GL_EXT_framebuffer_sRGB")) {Result.GL_EXT_framebuffer_sRGB=true;}
        else if(StringsAreEqual(Count, At, "GL_ARB_framebuffer_sRGB")) {Result.GL_EXT_framebuffer_sRGB=true;}
        // TODO(casey): Is there some kind of ARB string to look for that indicates GL_EXT_texture_sRGB?

        At = End;
    }

    return(Result);
}

internal void
OpenGLInit(b32 ModernContext, b32 FramebufferSupportsSRGB)
{
    opengl_info Info = OpenGLGetInfo(ModernContext);

    // NOTE(casey): If we believe we can do full sRGB on the texture side
    // and the framebuffer side, then we can enable it, otherwise it is
    // safer for us to pass it straight through.
    OpenGLDefaultInternalTextureFormat = GL_RGBA8;
    if(FramebufferSupportsSRGB && Info.GL_EXT_texture_sRGB && 
            Info.GL_EXT_framebuffer_sRGB)
    {
        OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
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

inline void
OpenGLDisplayBitmap(s32 Width, s32 Height, void *Memory, int Pitch,
    s32 WindowWidth, s32 WindowHeight, GLuint BlitTexture)
{
    Assert(Pitch == (Width*4));
    glViewport(0, 0, Width, Height);

    glDisable(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D, BlitTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, Width, Height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, Memory);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);    
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);

    glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
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
}

internal void
OpenGLRenderCommands(game_render_commands *Commands, s32 WindowWidth, s32 WindowHeight)
{    
    glViewport(0, 0, Commands->Width, Commands->Height);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    OpenGLSetScreenspace(Commands->Width, Commands->Height);

    u32 SortEntryCount = Commands->PushBufferElementCount;
    sort_entry *SortEntries = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);

    u32 ClipRectIndex = 0xFFFFFFFF;
    sort_entry *SortEntry = SortEntries;
    for(u32 SortEntryIndex = 0;
        SortEntryIndex < SortEntryCount;
        ++SortEntryIndex, ++SortEntry)
    {
        render_group_entry_header *Header = (render_group_entry_header *)
            (Commands->PushBufferBase + SortEntry->Index);
        if(ClipRectIndex != Header->ClipRectIndex)
        {
            ClipRectIndex = Header->ClipRectIndex;
            Assert(ClipRectIndex < Commands->ClipRectCount);
            
            render_entry_cliprect *Clip = Commands->ClipRects + ClipRectIndex;
            glScissor(Clip->Rect.MinX, Clip->Rect.MinY, 
                      Clip->Rect.MaxX - Clip->Rect.MinX, 
                      Clip->Rect.MaxY - Clip->Rect.MinY);
        }

        void *Data = (uint8 *)Header + sizeof(*Header);
        switch(Header->Type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *Entry = (render_entry_clear *)Data;

                glClearColor(Entry->Color.r, Entry->Color.g, Entry->Color.b, Entry->Color.a);
                glClear(GL_COLOR_BUFFER_BIT);
            } break;

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
                    r32 OneTexelU = 1.0f / (r32)Entry->Bitmap->Width;
                    r32 OneTexelV = 1.0f / (r32)Entry->Bitmap->Height;
                    v2 MinUV = V2(OneTexelU, OneTexelV);
                    v2 MaxUV = V2(1.0f - OneTexelU, 1.0f - OneTexelV);
                    OpenGLRectangle(Entry->P, MaxP, Entry->Color, MinUV, MaxUV);
                }
            } break;

            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                glDisable(GL_TEXTURE_2D);
                OpenGLRectangle(Entry->P, Entry->P + Entry->Dim, Entry->Color);
                glEnable(GL_TEXTURE_2D);
            } break;

            case RenderGroupEntryType_render_entry_line:
            {
                render_entry_line *Entry = (render_entry_line *)Data;
                glDisable(GL_TEXTURE_2D);
                OpenGLLine(Entry->P0, Entry->P1, Entry->Color);
                glEnable(GL_TEXTURE_2D);
            } break;

            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system *Entry = (render_entry_coordinate_system *)Data;
            } break;

            InvalidDefaultCase;
        }
    }
}

PLATFORM_ALLOCATE_TEXTURE(AllocateTexture)
{
    GLuint Handle;
    glGenTextures(1, &Handle);
    glBindTexture(GL_TEXTURE_2D, Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, 
        OpenGLDefaultInternalTextureFormat, 
        Width, Height, 0,
        GL_BGRA_EXT, GL_UNSIGNED_BYTE, Data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);    
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glBindTexture(GL_TEXTURE_2D, 0);

    glFlush();

    Assert(sizeof(Handle) <= sizeof(void *));
    return(PointerFromU32(void, Handle));
}

PLATFORM_DEALLOCATE_TEXTURE(DeallocateTexture) 
{
    GLuint Handle = U32FromPointer(Texture);
    glDeleteTextures(1, &Handle);
}

