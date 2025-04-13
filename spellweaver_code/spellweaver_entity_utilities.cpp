/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

inline move_spec
DefaultMoveSpec(void)
{
    move_spec Result;
    Result.UnitMaxAccelVector = false;
    Result.Speed = 1.0f;
    Result.Drag = 0.0f;

    return(Result);
}

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

inline found_entity
FindClosestEntityOfType(sim_region *SimRegion, entity *Entity, entity_type Type, r32 Range)
{
    found_entity Result = {};

    Result.DistanceSq = Square(Range);
    entity *TestEntity = SimRegion->Entities;
    for(uint32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        ++TestEntityIndex, ++TestEntity)
    {
        if(TestEntity->Type == Type)
        {            
            real32 TestDSq = LengthSq(TestEntity->P - Entity->P);            
            if(Result.DistanceSq > TestDSq)
            {
                Result.Entity = TestEntity;
                Result.DistanceSq = TestDSq;
            }
        }
    }

    return(Result);
}

inline found_entity
FindClosestEntityOfGeneralType(sim_region *SimRegion, entity *Entity, entity_general_type Type, r32 Range)
{
    found_entity Result = {};

    Result.DistanceSq = Square(Range);
    entity *TestEntity = SimRegion->Entities;
    for(uint32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        ++TestEntityIndex, ++TestEntity)
    {
        if(TestEntity->GeneralType == Type)
        {            
            real32 TestDSq = LengthSq(TestEntity->P - Entity->P);            
            if(Result.DistanceSq > TestDSq)
            {
                Result.Entity = TestEntity;
                Result.DistanceSq = TestDSq;
            }
        }
    }

    return(Result);
}

inline void
AddTimerForAttack(entity *Entity, r32 Duration, attack_type Type)
{
    timer *Timer = Entity->Timers + Entity->TimerCount;
    Timer->Finished = true;
    Timer->DurationSeconds = Duration;
    Timer->CurrentTime = Duration;
    Entity->AttackTimerIndex[Type] = Entity->TimerCount;
    ++Entity->TimerCount;
}

inline void
AddTimerForCast(entity *Entity, r32 DurationSeconds, castspell_type Type)
{
    timer *Timer = Entity->Timers + Entity->TimerCount;
    Timer->Finished = true;
    Timer->DurationSeconds = DurationSeconds;
    Timer->CurrentTime = DurationSeconds;
    Entity->CastSpellTimerIndex[Type] = Entity->TimerCount;
    ++Entity->TimerCount;
}

inline u32
GetTimerIndexForCastType(entity *Entity)
{
    u32 Result = Entity->CastSpellTimerIndex[Entity->CastSpellType];
    return(Result);
}

inline u32
GetTimerIndexForAttackType(entity *Entity)
{
    u32 Result = Entity->AttackTimerIndex[Entity->CastSpellType];
    return(Result);
}

inline b32
CheckTimerForAttack(entity *Entity)
{
    u32 AttackTimerIndex = Entity->AttackTimerIndex[Entity->AttackType];
    b32 Result = Entity->Timers[AttackTimerIndex].Finished;

    return(Result);
}

inline b32
CheckTimerForCastspell(entity *Entity)
{
    u32 CastspellTimerIndex = Entity->CastSpellTimerIndex[Entity->CastSpellType];
    b32 Result = Entity->Timers[CastspellTimerIndex].Finished;

    return(Result);
}

inline b32
CheckTimer(timer *Timer)
{
    b32 Result = Timer->Finished;

    return(Result);
}

inline void
ResetTimer(timer *Timer)
{
    Timer->Finished = false;
    Timer->CurrentTime = 0.0f;
}
