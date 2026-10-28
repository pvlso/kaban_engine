/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_render_group.h"

/*
  NOTE(pvlso): OpenGL 3.3 core. Nothing is drawn with glBegin: the render commands and nuklear
  both fill vertex buffers and draw them with shaders. The render commands go into one batch that
  is drawn whenever the texture, the kind of primitive, the scissor, the blend or the target
  changes. Programs, buffers and textures are shared with the debug window's context, vertex
  arrays aren't, so each nuklear window makes its own on its first draw.
*/

struct opengl_info
{
    b32 ModernContext;

    char *Vendor;
    char *Renderer;
    char *Version;
    char *ShadingLanguageVersion;

    s32 Major;
    s32 Minor;
};

struct opengl_vertex
{
    v2 P;
    v2 UV;
    v4 Color;
};

struct opengl_program
{
    GLuint Handle;
    GLint TransformID;
    GLint TextureID;
    GLint NormalTextureID; // NOTE(pvlso): texture unit 1
};

#define OPENGL_MAX_BATCH_VERTICES (6*16384)
struct opengl_batch
{
    GLuint VertexArray;
    GLuint VertexBuffer;

    GLenum Primitive;
    GLuint Texture;
    GLuint NormalTexture;
    u32 VertexCount;
    opengl_vertex Vertices[OPENGL_MAX_BATCH_VERTICES];
};

global_variable opengl_program OpenGLBasicProgram;
global_variable GLuint OpenGLWhiteTexture;
// NOTE(pvlso): A 1x1 normal map facing the viewer, for everything without one
global_variable GLuint OpenGLFlatNormalTexture;
global_variable opengl_batch OpenGLBatch;

// NOTE(pvlso): Untextured things draw with a white texture, so one program draws everything
global_variable char *OpenGLBasicVertexCode = R"GLSL(
#version 330 core
uniform mat4 Transform;
layout(location = 0) in vec2 InP;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;
out vec2 FragUV;
out vec4 FragColor;
void main()
{
    gl_Position = Transform*vec4(InP, 0.0, 1.0);
    FragUV = InUV;
    FragColor = InColor;
}
)GLSL";

// NOTE(pvlso): Into images with a normal buffer (the pixel world's) it writes the normal too, weighted like the colour
// so blending mixes them the same way; elsewhere that output goes nowhere. Transparent normal texels count as flat.
global_variable char *OpenGLBasicFragmentCode = R"GLSL(
#version 330 core
uniform sampler2D Texture;
uniform sampler2D NormalTexture;
in vec2 FragUV;
in vec4 FragColor;
layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec4 OutNormal;
void main()
{
    OutColor = texture(Texture, FragUV)*FragColor;
    vec4 Normal = texture(NormalTexture, FragUV);
    OutNormal = vec4((Normal.a > 0.0) ? Normal.rgb : vec3(0.5, 0.5, 1.0), 1.0)*OutColor.a;
}
)GLSL";

// NOTE(pvlso): The light pass: the world on screen (colour and normals) lit by the ambient and the lights, worked
// out per world pixel (PixelOrigin, PixelScale, like a game shader), so the pixels stay square. Lights are in the
// world image's pixels: x, y, height over the ground, radius. The same light as tools/art/normal_test.py.
global_variable char *OpenGLLightFragmentCode = R"GLSL(
#version 330 core
uniform sampler2D Texture;
uniform sampler2D NormalTexture;
uniform vec2 ScreenSize;
uniform float PixelScale;
uniform vec2 PixelOrigin;
uniform vec3 Ambient;
uniform int LightCount;
uniform vec4 Lights[64];
uniform vec3 LightColors[64];
in vec2 FragUV;
in vec4 FragColor;
out vec4 OutColor;
void main()
{
    vec4 Albedo = texture(Texture, FragUV);
    vec3 Normal = normalize(texture(NormalTexture, FragUV).rgb*2.0 - 1.0);
    vec2 P = floor((FragUV*ScreenSize - PixelOrigin) / PixelScale) + 0.5;
    vec3 Light = Ambient;
    for(int Index = 0; Index < LightCount; ++Index)
    {
        vec3 ToLight = vec3(Lights[Index].xy - P, Lights[Index].z);
        float Distance = max(length(ToLight), 0.001);
        float Facing = clamp(dot(Normal, ToLight) / Distance, 0.0, 1.0);
        float Falloff = clamp(1.0 - Distance / Lights[Index].w, 0.0, 1.0);
        Light += Facing*Falloff*Falloff*LightColors[Index];
    }
    OutColor = vec4(Albedo.rgb*Light, Albedo.a);
}
)GLSL";

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

    Result.Major = 1;
    Result.Minor = 0;
    if(MinorAt)
    {
        Result.Major = S32FromZ(MajorAt);
        Result.Minor = S32FromZ(MinorAt);
    }

    return(Result);
}

#if EDITOR_INTERNAL
global_variable u32 OpenGLDebugMessageCount;

// NOTE(pvlso): Only the first few, a mistake made every frame would fill the log otherwise
internal void APIENTRY
OpenGLDebugCallback(GLenum Source, GLenum Type, GLuint ID, GLenum Severity,
                    GLsizei Length, const GLchar *Message, const void *UserParam)
{
    if((Severity == GL_DEBUG_SEVERITY_HIGH) || (Severity == GL_DEBUG_SEVERITY_MEDIUM))
    {
        if(OpenGLDebugMessageCount < 16)
        {
            ++OpenGLDebugMessageCount;
            LOG_ERROR("OpenGL: %s", Message);
        }
    }
}
#endif

internal opengl_info
OpenGLInit(b32 ModernContext, b32 FramebufferSupportsSRGB)
{
    opengl_info Info = OpenGLGetInfo(ModernContext);

    // NOTE(casey): If we believe we can do full sRGB on the texture side
    // and the framebuffer side, then we can enable it, otherwise it is
    // safer for us to pass it straight through.
    // NOTE(pvlso): sRGB textures are core since 2.1, only the framebuffer is checked
    OpenGLDefaultInternalTextureFormat = GL_RGBA8;
    if(FramebufferSupportsSRGB)
    {
        OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;

        glEnable(GL_FRAMEBUFFER_SRGB);
    }

#if EDITOR_INTERNAL
    if(glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLDebugCallback, 0);
    }
#endif

    return(Info);
}

// NOTE(pvlso): The code can come in parts (Lengths 0 when they're all 0 terminated)
internal GLuint
OpenGLCompileShader(GLenum Type, GLsizei PartCount, char **Parts, GLint *Lengths, b32 *Compiled = 0)
{
    GLuint Result = glCreateShader(Type);
    glShaderSource(Result, PartCount, Parts, Lengths);
    glCompileShader(Result);

    GLint Status = 0;
    glGetShaderiv(Result, GL_COMPILE_STATUS, &Status);
    if(!Status)
    {
        char InfoLog[1024];
        glGetShaderInfoLog(Result, sizeof(InfoLog), 0, InfoLog);
        LOG_ERROR("Shader didn't compile: %s", InfoLog);
    }

    if(Compiled)
    {
        *Compiled = (Status != 0);
    }

    return(Result);
}

inline GLuint
OpenGLCompileShader(GLenum Type, char *Code)
{
    GLuint Result = OpenGLCompileShader(Type, 1, &Code, 0);
    return(Result);
}

// NOTE(pvlso): Handle is 0 when it didn't link
internal opengl_program
OpenGLLinkProgram(GLuint VertexShader, GLuint FragmentShader)
{
    opengl_program Result = {};

    Result.Handle = glCreateProgram();
    glAttachShader(Result.Handle, VertexShader);
    glAttachShader(Result.Handle, FragmentShader);
    glLinkProgram(Result.Handle);
    glDetachShader(Result.Handle, VertexShader);
    glDetachShader(Result.Handle, FragmentShader);
    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);

    GLint Linked = 0;
    glGetProgramiv(Result.Handle, GL_LINK_STATUS, &Linked);
    if(!Linked)
    {
        char InfoLog[1024];
        glGetProgramInfoLog(Result.Handle, sizeof(InfoLog), 0, InfoLog);
        LOG_ERROR("Shader program didn't link: %s", InfoLog);
        glDeleteProgram(Result.Handle);
        Result.Handle = 0;
    }
    else
    {
        Result.TransformID = glGetUniformLocation(Result.Handle, "Transform");
        Result.TextureID = glGetUniformLocation(Result.Handle, "Texture");
        Result.NormalTextureID = glGetUniformLocation(Result.Handle, "NormalTexture");
    }

    return(Result);
}

internal opengl_program
OpenGLCreateProgram(char *VertexCode, char *FragmentCode)
{
    GLuint VertexShader = OpenGLCompileShader(GL_VERTEX_SHADER, VertexCode);
    GLuint FragmentShader = OpenGLCompileShader(GL_FRAGMENT_SHADER, FragmentCode);
    opengl_program Result = OpenGLLinkProgram(VertexShader, FragmentShader);

    return(Result);
}

// NOTE(pvlso): Games' shaders (render_shader), compiled the first time a source is seen and kept by its hash, so an
// edited shader is a new one. One that doesn't compile is kept too, with Handle 0, so it's logged only once.
struct opengl_game_shader
{
    u64 Hash;
    opengl_program Program;
    GLint ScreenSizeID;
    GLint ImageSizeID;
    GLint PixelScaleID;
    GLint PixelOriginID;
    GLint TimeID;
    GLint ParamsID;
};

global_variable opengl_game_shader OpenGLGameShaders[8];
global_variable u32 OpenGLNextGameShader;

// NOTE(pvlso): Put before a game shader's code, the declarations SetPixelWorldShader promises (engine_api.h)
global_variable char *OpenGLGameShaderHeader = R"GLSL(#version 330 core
uniform sampler2D Texture;
uniform vec2 ScreenSize;
uniform vec2 ImageSize;
uniform float PixelScale;
uniform vec2 PixelOrigin;
uniform float Time;
uniform vec4 Params[4];
in vec2 FragUV;
in vec4 FragColor;
out vec4 OutColor;
#line 1
)GLSL";

internal opengl_game_shader *
OpenGLGetGameShader(render_shader *Shader)
{
    // NOTE(pvlso): FNV-1a
    u64 Hash = 14695981039346656037ULL;
    for(u32 Index = 0; Index < Shader->SourceLength; ++Index)
    {
        Hash ^= (u8)Shader->Source[Index];
        Hash *= 1099511628211ULL;
    }

    opengl_game_shader *Result = 0;
    for(u32 Index = 0; Index < ArrayCount(OpenGLGameShaders); ++Index)
    {
        if(OpenGLGameShaders[Index].Hash == Hash)
        {
            Result = OpenGLGameShaders + Index;
            break;
        }
    }

    if(!Result)
    {
        Result = OpenGLGameShaders + (OpenGLNextGameShader++ % ArrayCount(OpenGLGameShaders));
        if(Result->Program.Handle)
        {
            glDeleteProgram(Result->Program.Handle);
        }
        *Result = {};
        Result->Hash = Hash;

        char *Parts[] = {OpenGLGameShaderHeader, Shader->Source};
        GLint Lengths[] = {-1, (GLint)Shader->SourceLength};
        b32 Compiled = false;
        GLuint FragmentShader = OpenGLCompileShader(GL_FRAGMENT_SHADER, ArrayCount(Parts), Parts, Lengths, &Compiled);
        if(Compiled)
        {
            GLuint VertexShader = OpenGLCompileShader(GL_VERTEX_SHADER, OpenGLBasicVertexCode);
            Result->Program = OpenGLLinkProgram(VertexShader, FragmentShader);
            Result->ScreenSizeID = glGetUniformLocation(Result->Program.Handle, "ScreenSize");
            Result->ImageSizeID = glGetUniformLocation(Result->Program.Handle, "ImageSize");
            Result->PixelScaleID = glGetUniformLocation(Result->Program.Handle, "PixelScale");
            Result->PixelOriginID = glGetUniformLocation(Result->Program.Handle, "PixelOrigin");
            Result->TimeID = glGetUniformLocation(Result->Program.Handle, "Time");
            Result->ParamsID = glGetUniformLocation(Result->Program.Handle, "Params");
        }
        else
        {
            glDeleteShader(FragmentShader);
            LOG_ERROR("The game's world shader didn't compile, the world is drawn without it");
        }
    }

    return(Result);
}

// NOTE(pvlso): Position, UV and colour at locations 0, 1, 2, as the basic program reads them.
// The batch and nuklear lay out their vertices the same way.
internal void
OpenGLSetVertexLayout(GLsizei Stride, umm POffset, umm UVOffset, umm ColorOffset)
{
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, Stride, (void *)POffset);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, Stride, (void *)UVOffset);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, Stride, (void *)ColorOffset);
}

struct opengl_light_program
{
    opengl_program Program;
    GLint ScreenSizeID;
    GLint PixelScaleID;
    GLint PixelOriginID;
    GLint AmbientID;
    GLint LightCountID;
    GLint LightsID;
    GLint LightColorsID;
};
global_variable opengl_light_program OpenGLLightProgram;

// NOTE(pvlso): Called once, with the main window's context current
internal void
OpenGLCreateObjects(void)
{
    OpenGLBasicProgram = OpenGLCreateProgram(OpenGLBasicVertexCode, OpenGLBasicFragmentCode);

    opengl_light_program *Light = &OpenGLLightProgram;
    Light->Program = OpenGLCreateProgram(OpenGLBasicVertexCode, OpenGLLightFragmentCode);
    Light->ScreenSizeID = glGetUniformLocation(Light->Program.Handle, "ScreenSize");
    Light->PixelScaleID = glGetUniformLocation(Light->Program.Handle, "PixelScale");
    Light->PixelOriginID = glGetUniformLocation(Light->Program.Handle, "PixelOrigin");
    Light->AmbientID = glGetUniformLocation(Light->Program.Handle, "Ambient");
    Light->LightCountID = glGetUniformLocation(Light->Program.Handle, "LightCount");
    Light->LightsID = glGetUniformLocation(Light->Program.Handle, "Lights");
    Light->LightColorsID = glGetUniformLocation(Light->Program.Handle, "LightColors");

    // NOTE(pvlso): B G R A in memory: (0.5, 0.5, 1)
    u32 Flat = 0xFF8080FF;
    glGenTextures(1, &OpenGLFlatNormalTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGLFlatNormalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, &Flat);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    u32 White = 0xFFFFFFFF;
    glGenTextures(1, &OpenGLWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGLWhiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, &White);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    opengl_batch *Batch = &OpenGLBatch;
    glGenVertexArrays(1, &Batch->VertexArray);
    glGenBuffers(1, &Batch->VertexBuffer);
    glBindVertexArray(Batch->VertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, Batch->VertexBuffer);
    OpenGLSetVertexLayout(sizeof(opengl_vertex), OffsetOf(opengl_vertex, P),
                          OffsetOf(opengl_vertex, UV), OffsetOf(opengl_vertex, Color));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

internal void
OpenGLUseProgram(opengl_program *Program, r32 *Transform)
{
    glUseProgram(Program->Handle);
    glUniformMatrix4fv(Program->TransformID, 1, GL_FALSE, Transform);
    glUniform1i(Program->TextureID, 0);
    glUniform1i(Program->NormalTextureID, 1);
    glActiveTexture(GL_TEXTURE0);
}

// NOTE(pvlso): Pixels, (0, 0) bottom left to (Width, Height) top right. Column major.
inline void
OpenGLScreenspaceTransform(r32 *Transform, s32 Width, s32 Height)
{
    r32 a = SafeRatio1(2.0f, (r32)Width);
    r32 b = SafeRatio1(2.0f, (r32)Height);
    r32 Proj[] =
    {
         a,  0,  0,  0,
         0,  b,  0,  0,
         0,  0,  1,  0,
        -1, -1,  0,  1,
    };
    for(u32 Index = 0; Index < ArrayCount(Proj); ++Index)
    {
        Transform[Index] = Proj[Index];
    }
}

// NOTE(pvlso): Linear for what isn't colour (normal maps), read back as stored
internal void *
OpenGLAllocateTexture(u32 Width, u32 Height, void *Data, b32 Linear = false)
{
    GLuint Handle;
    glGenTextures(1, &Handle);
    glBindTexture(GL_TEXTURE_2D, Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, Linear ? GL_RGBA8 : OpenGLDefaultInternalTextureFormat,
                 Width, Height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, Data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    Assert(sizeof(Handle) <= sizeof(void *));
    void *Result = PointerFromU32(void, Handle);

    return(Result);
}

internal void
OpenGLFlushBatch(void)
{
    opengl_batch *Batch = &OpenGLBatch;
    if(Batch->VertexCount)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Batch->NormalTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Batch->Texture);
        glBindBuffer(GL_ARRAY_BUFFER, Batch->VertexBuffer);
        // NOTE(pvlso): A new store every time, so the driver doesn't wait for the last draw
        glBufferData(GL_ARRAY_BUFFER, Batch->VertexCount*sizeof(opengl_vertex), Batch->Vertices, GL_STREAM_DRAW);
        glDrawArrays(Batch->Primitive, 0, Batch->VertexCount);

        Batch->VertexCount = 0;
    }
}

// NOTE(pvlso): Room for VertexCount more vertices with this primitive and texture (and normal map, 0 for flat)
inline opengl_vertex *
OpenGLBatchVertices(GLenum Primitive, GLuint Texture, u32 VertexCount, GLuint NormalTexture = 0)
{
    if(!NormalTexture)
    {
        NormalTexture = OpenGLFlatNormalTexture;
    }

    opengl_batch *Batch = &OpenGLBatch;
    if((Batch->Primitive != Primitive) ||
       (Batch->Texture != Texture) ||
       (Batch->NormalTexture != NormalTexture) ||
       ((Batch->VertexCount + VertexCount) > ArrayCount(Batch->Vertices)))
    {
        OpenGLFlushBatch();
        Batch->Primitive = Primitive;
        Batch->Texture = Texture;
        Batch->NormalTexture = NormalTexture;
    }

    opengl_vertex *Result = Batch->Vertices + Batch->VertexCount;
    Batch->VertexCount += VertexCount;

    return(Result);
}

inline void
OpenGLVertex(opengl_vertex *Vertex, v2 P, v2 UV, v4 Color)
{
    Vertex->P = P;
    Vertex->UV = UV;
    Vertex->Color = Color;
}

inline void
OpenGLLine(v2 P0, v2 P1, v4 Color)
{
    opengl_vertex *V = OpenGLBatchVertices(GL_LINES, OpenGLWhiteTexture, 2);
    OpenGLVertex(V++, P0, V2(0, 0), Color);
    OpenGLVertex(V++, P1, V2(0, 0), Color);
}

inline void
OpenGLTriangle(v2 a, v2 b, v2 c, v4 Color)
{
    opengl_vertex *V = OpenGLBatchVertices(GL_TRIANGLES, OpenGLWhiteTexture, 3);
    OpenGLVertex(V++, a, V2(0, 0), Color);
    OpenGLVertex(V++, b, V2(0, 0), Color);
    OpenGLVertex(V++, c, V2(0, 0), Color);
}

inline void
OpenGLQuad(GLuint Texture, v2 P0, v2 P1, v2 P2, v2 P3,
           v4 Color, v2 MinUV = V2(0,0), v2 MaxUV = V2(1,1), GLuint NormalTexture = 0)
{
    opengl_vertex *V = OpenGLBatchVertices(GL_TRIANGLES, Texture, 6, NormalTexture);

    // Lower triangle: P0, P1, P2
    OpenGLVertex(V++, P0, V2(MinUV.x, MinUV.y), Color);
    OpenGLVertex(V++, P1, V2(MaxUV.x, MinUV.y), Color);
    OpenGLVertex(V++, P2, V2(MaxUV.x, MaxUV.y), Color);

    // Upper triangle: P0, P2, P3
    OpenGLVertex(V++, P0, V2(MinUV.x, MinUV.y), Color);
    OpenGLVertex(V++, P2, V2(MaxUV.x, MaxUV.y), Color);
    OpenGLVertex(V++, P3, V2(MinUV.x, MaxUV.y), Color);
}

inline void
OpenGLRectangle(GLuint Texture, v2 MinP, v2 MaxP, v4 Color, v2 MinUV = V2(0, 0), v2 MaxUV = V2(1, 1),
                GLuint NormalTexture = 0)
{
    OpenGLQuad(Texture, MinP, V2(MaxP.x, MinP.y), MaxP, V2(MinP.x, MaxP.y), Color, MinUV, MaxUV, NormalTexture);
}

inline void
OpenGLCircleOutline(v2 Center, f32 Radius, int Segments, v4 Color)
{
    v2 Last = V2(Center.x + Radius, Center.y);
    for(int i = 1;
        i <= Segments;
        ++i)
    {
        f32 a = (2.0f*Pi32)*((f32)i / (f32)Segments);
        v2 P = V2(Center.x + Radius*Cos(a), Center.y + Radius*Sin(a));

        OpenGLLine(Last, P, Color);
        Last = P;
    }
}

// NOTE(pvlso): An image drawn into, remade when its size changes
struct opengl_image
{
    GLuint Framebuffer;
    GLuint Texture;
    GLuint NormalTexture; // NOTE(pvlso): a second colour buffer for the normals, when made with them
    u32 Width;
    u32 Height;
};
// NOTE(pvlso): The pixel world's image (with its normals). When the game lights it or has a shader for it: the world
// on screen (the image with its upright things over it, with normals), and the lit world.
global_variable opengl_image OpenGLPixelWorld;
global_variable opengl_image OpenGLWorldOnScreen;
global_variable opengl_image OpenGLLitWorld;

// NOTE(pvlso): Which entries an OpenGLRenderEntries draws
enum opengl_entry_pass
{
    OpenGLPass_PixelWorld, // NOTE(pvlso): into the pixel world's image
    OpenGLPass_Upright,    // NOTE(pvlso): the pixel world's upright things, into OpenGLWorldOnScreen
    OpenGLPass_Screen,     // NOTE(pvlso): everything else, with the upright things unless the upright pass had them
};

// NOTE(pvlso): Where the image went on screen this frame (draw region pixels), for the upright pass
global_variable v2 OpenGLUprightMinP;
global_variable r32 OpenGLUprightScale;

// NOTE(pvlso): An image pixel position on the screen, in draw region pixels
inline v2
OpenGLUprightP(v2 P)
{
    v2 Result = OpenGLUprightMinP + OpenGLUprightScale*P;
    return(Result);
}

inline v2
RoundV2(v2 P)
{
    v2 Result = V2((r32)RoundReal32ToInt32(P.x), (r32)RoundReal32ToInt32(P.y));
    return(Result);
}

internal void
OpenGLMakeImage(opengl_image *World, u32 Width, u32 Height, b32 WithNormals = false)
{
    if((World->Width != Width) || (World->Height != Height))
    {
        if(!World->Framebuffer)
        {
            glGenFramebuffers(1, &World->Framebuffer);
        }
        if(World->Texture)
        {
            glDeleteTextures(1, &World->Texture);
        }
        if(World->NormalTexture)
        {
            glDeleteTextures(1, &World->NormalTexture);
            World->NormalTexture = 0;
        }

        World->Texture = U32FromPointer(OpenGLAllocateTexture(Width, Height, 0));
        World->Width = Width;
        World->Height = Height;

        glBindFramebuffer(GL_FRAMEBUFFER, World->Framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, World->Texture, 0);
        if(WithNormals)
        {
            World->NormalTexture = U32FromPointer(OpenGLAllocateTexture(Width, Height, 0, true));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, World->NormalTexture, 0);
            GLenum DrawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
            glDrawBuffers(ArrayCount(DrawBuffers), DrawBuffers);
        }
        GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        Assert(Status == GL_FRAMEBUFFER_COMPLETE);
    }
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
        glViewport(DrawRegion.Min.x, DrawRegion.Min.y, WindowWidth, WindowHeight);
    }
}

internal void OpenGLRenderEntries(editor_render_commands *Commands, editor_render_prep *Prep, rectangle2i DrawRegion,
                                  opengl_entry_pass Pass, b32 UprightDrawn = false);

// NOTE(pvlso): The bound image's normal buffer, to facing the viewer (glClear gave it the clear colour)
inline void
OpenGLClearNormals(void)
{
    GLfloat Flat[] = {0.5f, 0.5f, 1.0f, 1.0f};
    glClearBufferfv(GL_COLOR, 1, Flat);
}

internal void
OpenGLRenderCommands(editor_render_commands *Commands, editor_render_prep *Prep, rectangle2i DrawRegion,
                     u32 WindowWidth, u32 WindowHeight)
{
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    u32 MaxRenderTargetIndex = Commands->MaxRenderTargetIndex;
    if(MaxRenderTargetIndex >= GlobalFramebufferCount)
    {
        u32 NewFramebufferCount = Commands->MaxRenderTargetIndex + 1;
        Assert(NewFramebufferCount < ArrayCount(GlobalFramebufferHandles));
        u32 NewCount = NewFramebufferCount - GlobalFramebufferCount;
        glGenFramebuffers(NewCount, GlobalFramebufferHandles + GlobalFramebufferCount);

        for(u32 TargetIndex = GlobalFramebufferCount;
            TargetIndex < NewFramebufferCount;
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
        OpenGLBindFramebuffer(TargetIndex, DrawRegion);

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

    glUseProgram(OpenGLBasicProgram.Handle);
    glUniform1i(OpenGLBasicProgram.TextureID, 0);
    glUniform1i(OpenGLBasicProgram.NormalTextureID, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(OpenGLBatch.VertexArray);
    OpenGLBatch.VertexCount = 0;

    // NOTE(pvlso): The pixel world is drawn whole first, then put on the screen under everything else
    b32 UprightDrawn = false;
    render_pixel_world *PixelWorld = &Commands->PixelWorld;
    if(PixelWorld->Width && PixelWorld->Height)
    {
        OpenGLMakeImage(&OpenGLPixelWorld, PixelWorld->Width, PixelWorld->Height, true);
        glBindFramebuffer(GL_FRAMEBUFFER, OpenGLPixelWorld.Framebuffer);
        glViewport(0, 0, PixelWorld->Width, PixelWorld->Height);
        glScissor(0, 0, PixelWorld->Width, PixelWorld->Height);
        glClearColor(Commands->ClearColor.r, Commands->ClearColor.g,
                     Commands->ClearColor.b, Commands->ClearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
        OpenGLClearNormals();

        OpenGLRenderEntries(Commands, Prep, DrawRegion, OpenGLPass_PixelWorld);

        // NOTE(pvlso): In the screen's own pixels, so each image pixel is exactly Scale x Scale, centered and
        // moved by the scroll in whole screen pixels. The image's extra pixel on each side covers the move.
        s32 DrawWidth = GetWidth(DrawRegion);
        s32 DrawHeight = GetHeight(DrawRegion);
        s32 Scale = PixelWorld->Scale;
        s32 ScrollX = RoundReal32ToInt32(PixelWorld->Scroll.x*(r32)Scale);
        s32 ScrollY = RoundReal32ToInt32(PixelWorld->Scroll.y*(r32)Scale);
        v2 MinP = V2((r32)((DrawWidth - Scale*(s32)PixelWorld->Width) / 2 - ScrollX),
                     (r32)((DrawHeight - Scale*(s32)PixelWorld->Height) / 2 - ScrollY));
        v2 MaxP = MinP + V2((r32)(Scale*PixelWorld->Width), (r32)(Scale*PixelWorld->Height));
        // NOTE(pvlso): Upright things go where the image would be without rounding the scroll, so something the
        // camera follows stays on the same screen spot every frame (the ground is at most half a pixel off)
        OpenGLUprightMinP = V2((r32)((DrawWidth - Scale*(s32)PixelWorld->Width) / 2) - PixelWorld->Scroll.x*(r32)Scale,
                               (r32)((DrawHeight - Scale*(s32)PixelWorld->Height) / 2) - PixelWorld->Scroll.y*(r32)Scale);
        OpenGLUprightScale = (r32)Scale;

        r32 Transform[16];
        OpenGLScreenspaceTransform(Transform, DrawWidth, DrawHeight);
        opengl_game_shader *GameShader = PixelWorld->Shader.Source ? OpenGLGetGameShader(&PixelWorld->Shader) : 0;
        if(GameShader && !GameShader->Program.Handle)
        {
            GameShader = 0;
        }
        if(GameShader || PixelWorld->Lit)
        {
            // NOTE(pvlso): The image and the upright things go together into the world on screen first (colour and
            // normals), so the lights and the shader cover all of the world. Same pixels as the draw region,
            // without its offset.
            OpenGLMakeImage(&OpenGLWorldOnScreen, DrawWidth, DrawHeight, true);
            glBindFramebuffer(GL_FRAMEBUFFER, OpenGLWorldOnScreen.Framebuffer);
            glViewport(0, 0, DrawWidth, DrawHeight);
            glScissor(0, 0, DrawWidth, DrawHeight);
            glClear(GL_COLOR_BUFFER_BIT);
            OpenGLClearNormals();
            glUniformMatrix4fv(OpenGLBasicProgram.TransformID, 1, GL_FALSE, Transform);
            OpenGLRectangle(OpenGLPixelWorld.Texture, MinP, MaxP, V4(1, 1, 1, 1), V2(0, 0), V2(1, 1),
                            OpenGLPixelWorld.NormalTexture);
            OpenGLFlushBatch();
            OpenGLRenderEntries(Commands, Prep, DrawRegion, OpenGLPass_Upright);
            UprightDrawn = true;

            GLuint World = OpenGLWorldOnScreen.Texture;
            if(PixelWorld->Lit)
            {
                // NOTE(pvlso): Lights in arrays of the light program's layout
                r32 Lights[4*RENDER_MAX_LIGHTS];
                r32 LightColors[3*RENDER_MAX_LIGHTS];
                u32 LightCount = Minimum(PixelWorld->LightCount, (u32)RENDER_MAX_LIGHTS);
                for(u32 LightIndex = 0; LightIndex < LightCount; ++LightIndex)
                {
                    render_light *Light = PixelWorld->Lights + LightIndex;
                    Lights[4*LightIndex + 0] = Light->P.x;
                    Lights[4*LightIndex + 1] = Light->P.y;
                    Lights[4*LightIndex + 2] = Light->Height;
                    Lights[4*LightIndex + 3] = Light->Radius;
                    LightColors[3*LightIndex + 0] = Light->Color.r;
                    LightColors[3*LightIndex + 1] = Light->Color.g;
                    LightColors[3*LightIndex + 2] = Light->Color.b;
                }

                OpenGLMakeImage(&OpenGLLitWorld, DrawWidth, DrawHeight);
                glBindFramebuffer(GL_FRAMEBUFFER, OpenGLLitWorld.Framebuffer);
                glViewport(0, 0, DrawWidth, DrawHeight);
                glClear(GL_COLOR_BUFFER_BIT);

                opengl_light_program *Light = &OpenGLLightProgram;
                OpenGLUseProgram(&Light->Program, Transform);
                glUniform2f(Light->ScreenSizeID, (r32)DrawWidth, (r32)DrawHeight);
                glUniform1f(Light->PixelScaleID, (r32)Scale);
                glUniform2f(Light->PixelOriginID, MinP.x, MinP.y);
                glUniform3f(Light->AmbientID, PixelWorld->Ambient.r, PixelWorld->Ambient.g, PixelWorld->Ambient.b);
                glUniform1i(Light->LightCountID, (GLint)LightCount);
                if(LightCount)
                {
                    glUniform4fv(Light->LightsID, LightCount, Lights);
                    glUniform3fv(Light->LightColorsID, LightCount, LightColors);
                }
                OpenGLRectangle(World, V2(0, 0), V2((r32)DrawWidth, (r32)DrawHeight), V4(1, 1, 1, 1), V2(0, 0),
                                V2(1, 1), OpenGLWorldOnScreen.NormalTexture);
                OpenGLFlushBatch();
                World = OpenGLLitWorld.Texture;
            }

            OpenGLBindFramebuffer(0, DrawRegion);
            glScissor(DrawRegion.Min.x, DrawRegion.Min.y, DrawWidth, DrawHeight);
            if(GameShader)
            {
                render_shader_params *Params = &PixelWorld->Shader.Params;
                OpenGLUseProgram(&GameShader->Program, Transform);
                glUniform2f(GameShader->ScreenSizeID, (r32)DrawWidth, (r32)DrawHeight);
                glUniform2f(GameShader->ImageSizeID, (r32)PixelWorld->Width, (r32)PixelWorld->Height);
                glUniform1f(GameShader->PixelScaleID, (r32)Scale);
                glUniform2f(GameShader->PixelOriginID, MinP.x, MinP.y);
                glUniform1f(GameShader->TimeID, Params->Time);
                glUniform4fv(GameShader->ParamsID, ArrayCount(Params->Params), (r32 *)Params->Params);
            }
            else
            {
                OpenGLUseProgram(&OpenGLBasicProgram, Transform);
            }
            OpenGLRectangle(World, V2(0, 0), V2((r32)DrawWidth, (r32)DrawHeight), V4(1, 1, 1, 1));
            OpenGLFlushBatch();
            OpenGLUseProgram(&OpenGLBasicProgram, Transform);
        }
        else
        {
            OpenGLBindFramebuffer(0, DrawRegion);
            glScissor(DrawRegion.Min.x, DrawRegion.Min.y, DrawWidth, DrawHeight);
            glUniformMatrix4fv(OpenGLBasicProgram.TransformID, 1, GL_FALSE, Transform);
            OpenGLRectangle(OpenGLPixelWorld.Texture, MinP, MaxP, V4(1, 1, 1, 1));
            OpenGLFlushBatch();
        }
    }

    OpenGLRenderEntries(Commands, Prep, DrawRegion, OpenGLPass_Screen, UprightDrawn);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// NOTE(pvlso): The entries of one pass (opengl_entry_pass). Sets the transform for whichever target it draws to.
// UprightDrawn: the screen pass leaves out the upright things, the upright pass had them.
internal void
OpenGLRenderEntries(editor_render_commands *Commands, editor_render_prep *Prep, rectangle2i DrawRegion,
                    opengl_entry_pass Pass, b32 UprightDrawn)
{
    b32 PixelWorld = (Pass == OpenGLPass_PixelWorld);
    u32 MaxRenderTargetIndex = Commands->MaxRenderTargetIndex;
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
        Assert(Header->ClipRectIndex < Commands->ClipRectCount);
        u32 EntryTarget = Prep->ClipRects[Header->ClipRectIndex].RenderTargetIndex;
        opengl_entry_pass EntryPass = OpenGLPass_Screen;
        if(EntryTarget == RENDER_TARGET_PIXEL_WORLD)
        {
            EntryPass = OpenGLPass_PixelWorld;
        }
        else if((EntryTarget == RENDER_TARGET_PIXEL_WORLD_UPRIGHT) && UprightDrawn)
        {
            EntryPass = OpenGLPass_Upright;
        }
        if(Pass == OpenGLPass_Upright)
        {
            // NOTE(pvlso): Called only for it, so these are the upright things
            if(EntryTarget != RENDER_TARGET_PIXEL_WORLD_UPRIGHT)
            {
                continue;
            }
        }
        else if(EntryPass != Pass)
        {
            continue;
        }

        if(ClipRectIndex != Header->ClipRectIndex)
        {
            OpenGLFlushBatch();

            ClipRectIndex = Header->ClipRectIndex;

            render_entry_cliprect *Clip = Prep->ClipRects + ClipRectIndex;

            rectangle2i ClipRect = Clip->Rect;
            if(CurrentRenderTargetIndex != Clip->RenderTargetIndex)
            {
                CurrentRenderTargetIndex = Clip->RenderTargetIndex;

                r32 Transform[16];
                if(CurrentRenderTargetIndex == RENDER_TARGET_PIXEL_WORLD_UPRIGHT)
                {
                    // NOTE(pvlso): The entries are in image pixels, OpenGLUprightP puts them in these
                    if(Pass == OpenGLPass_Upright)
                    {
                        glBindFramebuffer(GL_FRAMEBUFFER, OpenGLWorldOnScreen.Framebuffer);
                        glViewport(0, 0, OpenGLWorldOnScreen.Width, OpenGLWorldOnScreen.Height);
                    }
                    else
                    {
                        OpenGLBindFramebuffer(0, DrawRegion);
                    }
                    OpenGLScreenspaceTransform(Transform, GetWidth(DrawRegion), GetHeight(DrawRegion));
                }
                else if(PixelWorld)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, OpenGLPixelWorld.Framebuffer);
                    glViewport(0, 0, OpenGLPixelWorld.Width, OpenGLPixelWorld.Height);
                    OpenGLScreenspaceTransform(Transform, OpenGLPixelWorld.Width, OpenGLPixelWorld.Height);
                }
                else
                {
                    Assert(CurrentRenderTargetIndex <= MaxRenderTargetIndex);
                    OpenGLBindFramebuffer(CurrentRenderTargetIndex, DrawRegion);
                    OpenGLScreenspaceTransform(Transform, Commands->Width, Commands->Height);
                }
                glUniformMatrix4fv(OpenGLBasicProgram.TransformID, 1, GL_FALSE, Transform);
            }

            if(Clip->RenderTargetIndex == RENDER_TARGET_PIXEL_WORLD_UPRIGHT)
            {
                ClipRect = DrawRegion;
                if(Pass == OpenGLPass_Upright)
                {
                    ClipRect = Offset(DrawRegion, -DrawRegion.Min.x, -DrawRegion.Min.y);
                }
            }
            else if(Clip->RenderTargetIndex == 0)
            {
                ClipRect = Offset(ClipRect, DrawRegion.Min.x, DrawRegion.Min.y);
            }

            glScissor(ClipRect.Min.x, ClipRect.Min.y, ClipRect.Max.x - ClipRect.Min.x, ClipRect.Max.y - ClipRect.Min.y);
        }

        // NOTE(pvlso): Upright entries of the pixel world are moved to the screen here. Bitmaps and rects start on
        // a whole screen pixel, so their texels and edges stay Scale wide.
        b32 Upright = (CurrentRenderTargetIndex == RENDER_TARGET_PIXEL_WORLD_UPRIGHT);
        r32 UprightScale = OpenGLUprightScale;

        void *Data = (uint8 *)Header + sizeof(*Header);
        switch(Header->Type)
        {
            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
                Assert(Entry->Bitmap);

                if(Entry->Bitmap->Width && Entry->Bitmap->Height)
                {
                    r32 c = cosf(Entry->RotateAngle);
                    r32 s = sinf(Entry->RotateAngle);

                    v2 XAxis = { c, s };
                    v2 YAxis = { -s, c };

                    v2 X = 0.5f*Entry->Size.x*XAxis;
                    v2 Y = 0.5f*Entry->Size.y*YAxis;
                    v2 C = Entry->P + 0.5f*Entry->Size;

                    v2 P0 = C - X - Y;
                    v2 P1 = C + X - Y;
                    v2 P2 = C + X + Y;
                    v2 P3 = C - X + Y;
                    if(Upright)
                    {
                        v2 Screen0 = OpenGLUprightP(P0);
                        v2 Snap = RoundV2(Screen0) - Screen0;
                        P0 = Screen0 + Snap;
                        P1 = OpenGLUprightP(P1) + Snap;
                        P2 = OpenGLUprightP(P2) + Snap;
                        P3 = OpenGLUprightP(P3) + Snap;
                    }

                    GLuint NormalTexture = Entry->NormalMap ? (GLuint)U32FromPointer(Entry->NormalMap->TextureHandle) : 0;
                    OpenGLQuad((GLuint)U32FromPointer(Entry->Bitmap->TextureHandle),
                               P0, P1, P2, P3, Entry->Color, Entry->MinUV, Entry->MaxUV, NormalTexture);
                }
            } break;

            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                v2 MinP = Entry->P;
                v2 MaxP = Entry->P + Entry->Dim;
                if(Upright)
                {
                    MinP = RoundV2(OpenGLUprightP(MinP));
                    MaxP = MinP + RoundV2(UprightScale*Entry->Dim);
                }
                OpenGLRectangle(OpenGLWhiteTexture, MinP, MaxP, SRGB1ToLinear1(Entry->Color));
            } break;

            case RenderGroupEntryType_render_entry_line:
            {
                render_entry_line *Entry = (render_entry_line *)Data;
                v2 P0 = Upright ? OpenGLUprightP(Entry->P0) : Entry->P0;
                v2 P1 = Upright ? OpenGLUprightP(Entry->P1) : Entry->P1;
                OpenGLLine(P0, P1, SRGB1ToLinear1(Entry->Color));
            } break;

            case RenderGroupEntryType_render_entry_triangle:
            {
                render_entry_triangle *Entry = (render_entry_triangle *)Data;
                v2 A = Upright ? OpenGLUprightP(Entry->A) : Entry->A;
                v2 B = Upright ? OpenGLUprightP(Entry->B) : Entry->B;
                v2 C = Upright ? OpenGLUprightP(Entry->C) : Entry->C;
                OpenGLTriangle(A, B, C, SRGB1ToLinear1(Entry->Color));
            } break;

            case RenderGroupEntryType_render_entry_circle_outline:
            {
                render_entry_circle_outline *Entry = (render_entry_circle_outline *)Data;
                v2 Center = Upright ? OpenGLUprightP(Entry->Center) : Entry->Center;
                r32 Radius = Upright ? UprightScale*Entry->Radius : Entry->Radius;
                OpenGLCircleOutline(Center, Radius, Entry->Segments, SRGB1ToLinear1(Entry->Color));
            } break;

            case RenderGroupEntryType_render_entry_blend_render_target:
            {
                render_entry_blend_render_target *Entry = (render_entry_blend_render_target *)Data;
                OpenGLFlushBatch();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                OpenGLRectangle(GlobalFramebufferTextures[Entry->SourceTargetIndex],
                                V2(0, 0), V2i(Commands->Width, Commands->Height), V4(1, 1, 1, Entry->Alpha));
                OpenGLFlushBatch();
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            } break;

            InvalidDefaultCase;
        }
    }

    OpenGLFlushBatch();
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
                                                               Op->Allocate.Data, Op->Allocate.Linear);
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
                 GL_BGRA, GL_UNSIGNED_BYTE, image);
}

struct nk_gl_vertex
{
    float position[2];
    float uv[2];
    float col[4];
};

internal void
NKOpenGLRenderCommands(nk_platform *NkPlatform, rectangle2i DrawRegion, enum nk_anti_aliasing AA)
{
    TIMED_FUNCTION();
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    struct nk_opengl *dev = &NkPlatform->ogl;

    // NOTE(pvlso): Made here, on the first draw, so the vertex array belongs to this window's context
    if(!dev->vao)
    {
        glGenVertexArrays(1, &dev->vao);
        glGenBuffers(1, &dev->vbo);
        glGenBuffers(1, &dev->ebo);
        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);
        OpenGLSetVertexLayout(sizeof(struct nk_gl_vertex), OffsetOf(struct nk_gl_vertex, position),
                              OffsetOf(struct nk_gl_vertex, uv), OffsetOf(struct nk_gl_vertex, col));
    }

    /* setup global state */
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* setup viewport/project */
    glViewport(DrawRegion.Min.x, DrawRegion.Min.y, (GLsizei)NkPlatform->display_width,(GLsizei)NkPlatform->display_height);

    // NOTE(pvlso): What glOrtho(0, width, height, 0, -1, 1) made, column major
    r32 Ortho[16] =
    {
        2.0f / (r32)NkPlatform->width, 0, 0, 0,
        0, -2.0f / (r32)NkPlatform->height, 0, 0,
        0, 0, -1, 0,
        -1, 1, 0, 1,
    };
    OpenGLUseProgram(&OpenGLBasicProgram, Ortho);
    glBindVertexArray(dev->vao);
    glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);

    {
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
        nk_convert(&NkPlatform->ctx, &dev->cmds, &vbuf, &ebuf, &config);

        /* upload the vertices and indices (the index buffer binding lives in the vertex array) */
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vbuf.allocated, nk_buffer_memory_const(&vbuf), GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ebuf.allocated, nk_buffer_memory_const(&ebuf), GL_STREAM_DRAW);

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, &NkPlatform->ctx, &dev->cmds)
        {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);

            float sx = cmd->clip_rect.x * NkPlatform->fb_scale.x;
            float sy = (NkPlatform->height - (cmd->clip_rect.y + cmd->clip_rect.h)) * NkPlatform->fb_scale.y;
            float sw = cmd->clip_rect.w * NkPlatform->fb_scale.x;
            float sh = cmd->clip_rect.h * NkPlatform->fb_scale.y;

            GLint x = (GLint)FloorReal32ToInt32(sx);
            GLint y = (GLint)FloorReal32ToInt32(sy);
            GLint w = (GLint)CeilReal32ToInt32(sw);
            GLint h = (GLint)CeilReal32ToInt32(sh);

            w += 1;
            h += 1;
            x += (GLint)CeilReal32ToInt32(DrawRegion.Min.x * NkPlatform->fb_scale.x);
            y += (GLint)CeilReal32ToInt32(DrawRegion.Min.y * NkPlatform->fb_scale.y);

            glScissor(x, y, w, h);
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(&NkPlatform->ctx);
        nk_buffer_clear(&dev->cmds);
        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }

    /* default OpenGL state */
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}
