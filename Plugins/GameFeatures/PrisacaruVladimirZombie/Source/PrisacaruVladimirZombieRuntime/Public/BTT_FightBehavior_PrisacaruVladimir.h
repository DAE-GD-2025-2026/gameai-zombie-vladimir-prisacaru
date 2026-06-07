// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Items/BaseItem.h"
#include "Village/House/House.h"
#include "BTT_FightBehavior_PrisacaruVladimir.generated.h"

/**
 * BTT_FightBehavior
 *
 * Handles ALL combat-branch logic in one persistent task (mirrors BTT_SafeBehavior).
 * Runs when bSeesZombies is set.  Never returns Failed — exits only when aborted.
 *
 * Internal priority each Idle tick:
 *   1. Has weapon + fightable zombies -> engage (move + attack)
 *   2. Has weapon + too many zombies -> flee-wander (run, bias AWAY from nearest zombie)
 *   3. No weapon, visible weapon item -> sprint to it and grab it
 *   4. No weapon, unsearched house -> sprint to nearest unsearched house → spin
 *   5. No weapon, all houses searched -> flee-wander (run, bias AWAY from nearest zombie)
 *
 * State machine:
 *   Idle -> pick action above
 *   Engaging -> move toward nearest zombie; attack if in range; re-evaluates every SteeringInterval
 *   WalkingToItem -> sprinting to a weapon item; grab on arrival
 *   WalkingToHouse -> sprinting to unsearched house; on arrival -> Spinning
 *   Spinning -> 360° scan; on completion -> Idle
 *   FleeWander -> wander-steered running, angle biased AWAY from nearest zombie each tick
 */
UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_FightBehavior_PrisacaruVladimir : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTT_FightBehavior_PrisacaruVladimir();

	// Combat
	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="1"))
	int32 MaxZombiesToFight = 2;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="1.0"))
	float FastZombieSpeedThreshold = 350.f;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="0.99"))
	float SpeedSmoothingAlpha = 0.7f;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="10.0"))
	float AttackRange = 200.f;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.01"))
	float SteeringInterval = 0.15f;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="10.0"))
	float MoveAcceptanceRadius = 80.f;

	// House search
	UPROPERTY(EditAnywhere, Category="HouseSearch", meta=(ClampMin="10.0"))
	float HouseArrivalRadius = 200.f;

	UPROPERTY(EditAnywhere, Category="HouseSearch", meta=(ClampMin="0.0"))
	float HouseMoveTimeout = 20.f;

	UPROPERTY(EditAnywhere, Category="HouseSearch", meta=(ClampMin="0.1"))
	float SpinDuration = 1.5f;

	// Item collection
	UPROPERTY(EditAnywhere, Category="Items", meta=(ClampMin="0.0"))
	float ItemMoveTimeout = 15.f;

	// Flee-wander (unarmed or overwhelmed)
	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="100.0"))
	float WanderRadius = 800.f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="0.5"))
	float WanderDestinationInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="0.01"))
	float WanderSteeringInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="10.0"))
	float WanderCircleOffset = 150.f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="10.0"))
	float WanderCircleRadius = 100.f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="0.01"))
	float WanderMaxAngleChange = 0.4f;

	/* How strongly the wander angle is pushed AWAY from the nearest zombie per steering update. */
	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="0.0"))
	float ZombieRepulsionBias = 0.8f;

	UPROPERTY(EditAnywhere, Category="FleeWander", meta=(ClampMin="10.0"))
	float WanderAcceptanceRadius = 50.f;

	
	
	protected:
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory, float DeltaSeconds) override;
	
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual uint16 GetInstanceMemorySize() const override;
};

// Speed tracking
struct FFightSpeedEntry
{
	TWeakObjectPtr<class ABaseZombie> Zombie;
	FVector LastPosition { FVector::ZeroVector };
	float SmoothedSpeed { 0.f };
};

enum class EFightBehaviorPhase : uint8
{
	Idle,
	Engaging,
	WalkingToItem,
	WalkingToHouse,
	Spinning,
	FleeWander,
};

struct FFightBehaviorMemory
{
	EFightBehaviorPhase Phase { EFightBehaviorPhase::Idle };
	TWeakObjectPtr<ABaseItem> TargetItem;
	TWeakObjectPtr<AHouse> TargetHouse;
	float PhaseTimer { 0.f };
	float SpinStartYaw { 0.f };
	
	// Engaging / steering
	float TimeSinceSteering { 0.f };
	TArray<FFightSpeedEntry> SpeedEntries;
	
	// FleeWander
	FVector WanderDestination { FVector::ZeroVector };
	float WanderAngle { 0.f };
	float TimeSinceWanderDest  { 0.f };
	float TimeSinceWanderSteer { 0.f };
};