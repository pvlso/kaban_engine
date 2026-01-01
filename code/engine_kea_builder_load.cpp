/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#if 0
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

inline loaded_bitmap
STBLoadImage(char *FileName, platform_file_type Type, memory_arena *Arena)
{
    loaded_bitmap Result = {};

    read_file_result ReadResult = Platform.ReadEntireFile(FileName, Type, Arena, 0);    
    if(ReadResult.Size != 0)
    {
        s32 Comp = 0;
        stbi_set_flip_vertically_on_load(1);
        u8 *Pixels = stbi_load_from_memory((u8 *)ReadResult.Contents, ReadResult.Size,
                                           &Result.Width, &Result.Height,
                                           &Comp, BITMAP_BYTES_PER_PIXEL);

        Result.WidthOverHeight = (r32)Result.Width / (r32)Result.Height;
        Result.AlignPercentage = V2(0.5f, 0.5f);

//        Assert(Comp == BITMAP_BYTES_PER_PIXEL);
        Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;

        u32 *Dest = 0;
        if(Arena)
            Dest = PushArray(Arena, Result.Width*Result.Height, u32);
        else
            Dest = (u32 *)Platform.AllocateMemory(Result.Width*Result.Height*sizeof(u32));
        Result.Memory = Dest;

        u32 PixelCount = Result.Width*Result.Height;
        for(u32 I = 0;
            I < PixelCount;
            ++I)
        {
            u8 r8 = Pixels[4*I + 0];
            u8 b8 = Pixels[4*I + 1];
            u8 g8 = Pixels[4*I + 2];
            u8 a8 = Pixels[4*I + 3];

            v4 Texel = V4(r8, b8, g8, a8);

            Texel = SRGB255ToLinear1(Texel);
            Texel.rgb *= Texel.a;
            Texel = Linear1ToSRGB255(Texel);

            u32 A = (u32)(Texel.a + 0.5f);
            u32 R = (u32)(Texel.r + 0.5f);
            u32 G = (u32)(Texel.g + 0.5f);
            u32 B = (u32)(Texel.b + 0.5f);

            Dest[I] = ((A << 24) | (R << 16) | (G << 8) | (B << 0));
        }

        if(!Arena)
            Platform.FreeFileMemory(ReadResult.Contents);

        stbi_image_free(Pixels);
    }
    
    return(Result);
}
#endif

internal loaded_bitmap
LoadBMP(char *FileName, platform_file_type Type, memory_arena *Arena)
{
    loaded_bitmap Result = {};
    
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, Type, Arena, 0);    
    if(ReadResult.Size != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        Result.WidthOverHeight = (r32)Result.Width / (r32)Result.Height;
        Result.AlignPercentage = V2(0.5f, 0.5f);
        
        Assert(Result.Height >= 0);
        Assert(Header->Compression == 3);

        // NOTE(casey): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!!)

        // NOTE(casey): Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown = (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;
        
        uint32 *SourceDest = Pixels;
        for(int32 Y = 0;
            Y < Header->Height;
            ++Y)
        {
            for(int32 X = 0;
                X < Header->Width;
                ++X)
            {
                uint32 C = *SourceDest;

                v4 Texel  =
                    {
                        (real32)((C & RedMask) >> RedShiftDown),
                        (real32)((C & GreenMask) >> GreenShiftDown),
                        (real32)((C & BlueMask) >> BlueShiftDown),
                        (real32)((C & AlphaMask) >> AlphaShiftDown)
                    };

                Texel = SRGB255ToLinear1(Texel);

                Texel.rgb *= Texel.a;
                Texel = Linear1ToSRGB255(Texel);
                
                *SourceDest++ = (((uint32)(Texel.a + 0.5f) << 24) |
                                 ((uint32)(Texel.r + 0.5f) << 16) |
                                 ((uint32)(Texel.g + 0.5f) << 8) |
                                 ((uint32)(Texel.b + 0.5f) << 0));
            }
        }
    }

    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    
    return(Result);
}

struct riff_iterator
{
    uint8 *At;
    uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
    riff_iterator Iter;

    Iter.At = (uint8 *)At;
    Iter.Stop = (uint8 *)Stop;

    return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;

    return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop);

    return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void *Result = (Iter.At + sizeof(WAVE_chunk)); 

    return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->ID;

    return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->Size;

    return(Result);
}

internal loaded_sound
LoadWAV(char *FileName, u32 SectionFirstSampleIndex, u32 SectionSampleCount, void **Free, memory_arena *Arena)
{
    loaded_sound Result = {};
    
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_WAV, Arena, 0);    
    if(ReadResult.Size != 0)
    {
        if(!Arena)
        {
            *Free = ReadResult.Contents;
        }

        WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16 *SampleData = 0;
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetType(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(fmt->wFormatTag == 1); // NOTE(casey): Only support PCM
                    Assert(fmt->nSamplesPerSec == 48000);
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == sizeof(int16)*fmt->nChannels);
                    ChannelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    SampleDataSize = GetChunkDataSize(Iter);
                } break;
            }
        }

        Assert(ChannelCount && SampleData);

        Result.ChannelCount = ChannelCount;
        u32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
        if(ChannelCount == 1)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        }
        else if(ChannelCount == 2)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;

            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                ++SampleIndex)
            {
                int16 Source = SampleData[2*SampleIndex];
                SampleData[2*SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO(casey): Load right channels!
        b32 AtEnd = true;
        Result.ChannelCount = 1;
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            SampleCount = SectionSampleCount;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }

        if(AtEnd)
        {
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                for(u32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 8);
                    ++SampleIndex)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}

internal builder_loaded_sprites
LoadSprites(kesa_asset *KESAAsset, memory_arena *Arena)
{
    builder_loaded_sprites Result = {};
    loaded_bitmap SpriteSheetBitmap = LoadBMP(KESAAsset->SourceFileName, PlatformFileType_SSBMP, Arena);

    if(SpriteSheetBitmap.Memory)
    {
        Result.Count = KESAAsset->SpriteSheet.SpriteCount;
        Result.Sprites = PushArray(Arena, KESAAsset->SpriteSheet.SpriteCount, loaded_bitmap);
        for(u32 SpriteIndex = 0;
            SpriteIndex < KESAAsset->SpriteSheet.SpriteCount;
            ++SpriteIndex)
        {
            loaded_bitmap *Sprite = Result.Sprites + SpriteIndex;
            Sprite->Width = KESAAsset->SpriteSheet.SpriteWidth;
            Sprite->Height = KESAAsset->SpriteSheet.SpriteHeight;
            Sprite->WidthOverHeight = (r32)Sprite->Width/(r32)Sprite->Height;
            Sprite->Pitch = Sprite->Width*BITMAP_BYTES_PER_PIXEL;
            Sprite->AlignPercentage = V2(0.5f, 0.5f);
            u32 MemorySize = Sprite->Pitch*Sprite->Height;
            Sprite->Memory = (Arena ? PushSize(Arena, MemorySize): Platform.AllocateMemory(MemorySize));

            u8 *SourceRow = (u8 *)(SpriteSheetBitmap.Memory) + SpriteIndex*Sprite->Pitch;
            u8 *DestRow = (u8 *)(Sprite->Memory);
            for(s32 Y = 0;
                Y < Sprite->Height;
                ++Y)
            {
                u32 *Source = (u32 *)SourceRow;
                u32 *Dest = (u32 *)DestRow;
                for(s32 X = 0;
                    X < Sprite->Width;
                    ++X)
                {
                    *Dest++ = *Source++;
                }

                SourceRow += SpriteSheetBitmap.Pitch;
                DestRow += Sprite->Pitch;
            }
        }
    }

    return(Result);
}

internal loaded_text
LoadText(char *FileName, memory_arena *Arena)
{
    loaded_text Result = {};
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_TXT, Arena, 0);    
    Result.String = (char *)ReadResult.Contents;

    return(Result);
}

internal loaded_bitmap
LoadTileBitmap(kesa_tileset *Tileset, loaded_bitmap *TilesetBitmap,
               loaded_bitmap *MergeTileBitmap, u32 TileIndex, memory_arena *TempArena)
{
    loaded_bitmap Tile = {};
    Tile.Width = Tileset->TileWidth;
    Tile.Height = Tileset->TileHeight;
    Tile.WidthOverHeight = (r32)Tile.Width/(r32)Tile.Height;
    Tile.AlignPercentage = V2(0.5f, 0.5f);
    Tile.Pitch = Tile.Width*BITMAP_BYTES_PER_PIXEL;
    u32 MemorySize = Tile.Height*Tile.Pitch;
    Tile.Memory = PushSize(TempArena, MemorySize);

    u8 *DestRow = (u8 *)Tile.Memory;

    u32 MainSurfaceIndexX = Tileset->TileOffsetsX[TileIndex]*Tile.Width;
    u32 MainSurfaceIndexY = Tileset->TileOffsetsY[TileIndex]*Tile.Height;
    u8 *MainSurfaceSource = (u8 *)TilesetBitmap->Memory + (MainSurfaceIndexY*TilesetBitmap->Pitch +
                                                                   MainSurfaceIndexX*BITMAP_BYTES_PER_PIXEL);
    if(Tileset->MergedTile)
    {
        loaded_bitmap *MergeTile = MergeTileBitmap;
        u8 *MergeSurfaceSource = (u8 *)MergeTile->Memory;
        for(s32 Y = 0;
            Y < Tile.Height;
            ++Y)
        {
            u32 *MergeSource = (u32 *)MergeSurfaceSource;
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile.Width;
                ++X)
            {
                if(*MainSource)
                {
                    *Dest = *MainSource;
                }
                else
                {
                    *Dest = *MergeSource;
                }

                ++MainSource;
                ++MergeSource;
                ++Dest;
            }

            MainSurfaceSource += TilesetBitmap->Pitch;
            MergeSurfaceSource += MergeTile->Pitch;
            DestRow += Tile.Pitch;
        }
    }
    else
    {
        for(s32 Y = 0;
            Y < Tile.Height;
            ++Y)
        {
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile.Width;
                ++X)
            {
                *Dest = *MainSource;

                ++MainSource;
                ++Dest;
            }

            MainSurfaceSource += TilesetBitmap->Pitch;
            DestRow += Tile.Pitch;
        }
    }

    return(Tile);
}
