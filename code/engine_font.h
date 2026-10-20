#if !defined(ENGINE_FONT_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Fonts for games, from .ttf files in the project's data/ttfs (stb_truetype).

  A font is loaded the first time a game asks for it by file name and stays until the game stops.
  Text is drawn at whole screen pixel sizes: the first time a size is drawn, its glyphs are packed
  into an atlas of their own and the atlas goes up as a texture through the texture op queue. Text
  at a size whose texture isn't up yet is skipped for that frame. So any camera zoom gets crisp
  text, at the cost of an atlas per size (MAX_FONT_SIZES per font, after that the nearest baked
  size is scaled).

  The atlas is premultiplied alpha with its rows bottom up, like the bitmaps. Glyphs are drawn as
  bitmap entries with their own UV rectangle.
*/

// NOTE(pvlso): Third party code, its warnings are off (the engine builds with -WX)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#define STBTT_STATIC
#include "stb_truetype.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// NOTE(pvlso): Every size bakes printable ASCII and Latin-1, the rest draws as '?'
#define FONT_ASCII_FIRST 32
#define FONT_ASCII_COUNT 95
#define FONT_LATIN1_FIRST 160
#define FONT_LATIN1_COUNT 96
#define FONT_GLYPH_COUNT (FONT_ASCII_COUNT + FONT_LATIN1_COUNT)

#define FONT_MIN_PIXEL_SIZE 6
#define FONT_MAX_PIXEL_SIZE 160
#define MAX_FONT_SIZES 12
#define MAX_GAME_FONTS 16

struct font_size
{
    s32 PixelSize;
    loaded_bitmap Atlas;
    stbtt_packedchar Glyphs[FONT_GLYPH_COUNT];
};

struct engine_font
{
    char FileName[128];
    // NOTE(pvlso): 0 when the file wasn't there or isn't a font, GetFont then keeps returning 0
    // without trying again
    void *FileMemory;
    stbtt_fontinfo Info;
    s32 Ascent;
    s32 Descent;
    s32 LineGap;

    u32 SizeCount;
    font_size *Sizes[MAX_FONT_SIZES];
    b32 WarnedSizesFull;
};

// NOTE(pvlso): The running game's fonts, in editor_mode_game
struct game_fonts
{
    // NOTE(pvlso): Where new atlases go up, set every frame before the game runs
    platform_texture_op_queue *TextureOpQueue;

    u32 FontCount;
    engine_font *Fonts[MAX_GAME_FONTS];
    b32 WarnedFontsFull;
};

#define ENGINE_FONT_H
#endif
