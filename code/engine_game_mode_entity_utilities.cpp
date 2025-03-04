/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline u32
GetAnimationTypeForCastType(castspell_type Type)
{
    u32 Result = 0;
    switch(Type)
    {
        case CastSpellType_0: {Result = AnimationType_CastSpell0;} break;
        case CastSpellType_1: {Result = AnimationType_CastSpell1;} break;
        case CastSpellType_2: {Result = AnimationType_CastSpell2;} break;

            InvalidDefaultCase;
    }

    return(Result);
}

inline u32
GetAnimationTypeForAttackType(attack_type Type)
{
    u32 Result = 0;
    switch(Type)
    {
        case AttackType_0: {Result = AnimationType_Attack0;} break;
        case AttackType_1: {Result = AnimationType_Attack1;} break;
        case AttackType_2: {Result = AnimationType_Attack2;} break;

            InvalidDefaultCase;
    }

    return(Result);
}

inline s32
GetSpriteIndex(r32 Time, u32 SpriteCount, u32 Offset, u32 Speed = 0)
{
    s32 Result = 0;
    if(Speed)
    {
        Result = FloorReal32ToInt32(Time*Speed) % SpriteCount;
        Result -= Offset;
    }
    else
    {
        Result = FloorReal32ToInt32(Time*SpriteCount) % SpriteCount;
        Result -= Offset;
    }

    if(Result < 0)
    {
        Result += SpriteCount;
    }
    
    return(Result);
}

inline b32
AnimationHasComleted(r32 Time, u32 Offset, u32 SpriteIndex, u32 SpriteCount, r32 dt, u32 Speed)
{
    u32 Count = (Speed != 0) ? Speed : SpriteCount;
    b32 Result = (SpriteIndex == (SpriteCount - 1) &&
                  GetSpriteIndex((Time + dt), SpriteCount, Offset, Speed) == 0);

    return(Result);
}

inline b32
AnimationFinishedOnSprite(r32 Time, u32 Offset, u32 SpriteIndex, u32 SpriteCount, r32 dt,
                          u32 StopSpriteIndex, u32 Speed)
{
    u32 OneFramePastSpriteIndex = GetSpriteIndex((Time + dt), SpriteCount, Offset, Speed);
    b32 Result = ((SpriteIndex == (StopSpriteIndex - 1)) &&
                  (OneFramePastSpriteIndex == StopSpriteIndex));

    return(Result);
}

inline v2
FacingDirectionToUnitVector(u32 FacingDirection)
{
    v2 Result = {};
    if(FacingDirection == 0)
    {
        Result = V2(1.0f, 0.0f);
    }
    else if(FacingDirection == 1)
    {
        Result = V2(0.0f, 1.0f);
    }
    else if(FacingDirection == 2)
    {
        Result = V2(-1.0f, 0.0f);
    }
    else if(FacingDirection == 3)
    {
        Result = V2(0.0f, -1.0f);
    }

    return(Result);
}

inline s32
FacingDirectionFromVector(v2 Vector)
{
    s32 Result = -1;
    
    if((AbsoluteValue(Vector.x) == 0.0f) && (AbsoluteValue(Vector.y) == 0.0f))
    {
        // NOTE(casey): Leave FacingDirection whatever it was
    }
    else if(AbsoluteValue(Vector.x) > AbsoluteValue(Vector.y))
    {
        if(Vector.x > 0.0f)
        {
            Result = 0;
        }
        else
        {
            Result = 2;
        }
    }
    else
    {
        if(Vector.y > 0.0f)
        {
            Result = 1;
        }
        else
        {
            Result = 3;
        }
    }

    return(Result);
}
