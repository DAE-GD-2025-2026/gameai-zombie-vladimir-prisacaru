// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Items/BaseItem.h"
#include "Village/House/House.h"
#include "BTT_SafeBehavior_PrisacaruVladimir.generated.h"

/**
 * BTT_SafeBehavior
 *
 * Handles ALL safe-branch logic in one persistent task
 * Runs when no zombies and no purge zone are present
 * Never returns Failed — it always has something to do (worst case: wander)
 * Returns only when aborted by a decorator (zombie/purge zone spotted)
 *
 * Internal priority each tick:
 *   1. HP critical -> use medkit/food from inventory, or walk to one, or go to house
 *   2. Stamina low -> use food from inventory, or walk to one, or go to house
 *   3. No weapon -> walk to visible weapon, or go to house
 *   4. Nothing needed -> smart wander
 *
 * Internal state machine:
 *   Idle -> pick the highest-priority action
 *   WalkingToItem -> moving toward a world item; grab on arrival
 *   WalkingToHouse -> moving toward unsearched house; on arrival -> Spinning
 *   Spinning -> rotating 360 degree to scan; on completion -> Idle
 *   Wandering -> smart wander movement; re-evaluates every WanderInterval
 */
UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_SafeBehavior_PrisacaruVladimir : public UBTTaskNode
{
	GENERATED_BODY()

	public:
	
	UBTT_SafeBehavior_PrisacaruVladimir();

	// Thresholds
	UPROPERTY(EditAnywhere, Category="Needs", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LowHealthThreshold = 0.4f;

	UPROPERTY(EditAnywhere, Category="Needs", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LowStaminaThreshold = 0.35f;

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

	// Wander
	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="100.0"))
	float WanderRadius = 800.f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="1"))
	int32 WanderCandidates = 8;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="50.0"))
	float WanderDensityRadius = 300.f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="0.1"))
	float WanderSteeringInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="0.5"))
	float WanderDestinationInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="10.0"))
	float WanderCircleOffset = 150.f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="10.0"))
	float WanderCircleRadius = 100.f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="0.01"))
	float WanderMaxAngleChange = 0.4f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="0.0"))
	float WanderDestinationBias = 0.3f;

	UPROPERTY(EditAnywhere, Category="Wander", meta=(ClampMin="10.0"))
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

enum class ESafeBehaviorPhase : uint8
{
	Idle,
	WalkingToItem,
	WalkingToHouse,
	Spinning,
	Wandering
};

struct FSafeBehaviorMemory
{
	ESafeBehaviorPhase Phase { ESafeBehaviorPhase::Idle };
	TWeakObjectPtr<ABaseItem> TargetItem;
	TWeakObjectPtr<AHouse> TargetHouse;
	float PhaseTimer { 0.f };
	float SpinStartYaw { 0.f };
	
	// Wander state
	FVector WanderDestination { FVector::ZeroVector };
	float WanderAngle { 0.f };
	float TimeSinceWanderDest { 0.f };
	float TimeSinceWanderSteer { 0.f };
};