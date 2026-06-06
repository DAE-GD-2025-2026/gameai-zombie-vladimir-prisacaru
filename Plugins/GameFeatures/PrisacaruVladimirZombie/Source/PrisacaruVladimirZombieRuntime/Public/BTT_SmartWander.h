// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_SmartWander.generated.h"



UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_SmartWander : public UBTTaskNode
{
	GENERATED_BODY()

	public:
	
	UBTT_SmartWander();
	
	// Destination picking

	/* How often a new unexplored destination is picked (seconds). */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="0.1"))
	float DestinationInterval = 1.5f;

	/* Radius within which destination candidates are sampled. */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="100.0"))
	float WanderRadius = 800.f;

	/* Number of candidates scored when picking a destination. */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="1", ClampMax="32"))
	int32 NumCandidates = 8;

	/* Radius used when querying exploration density around a candidate. */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="50.0"))
	float DensityQueryRadius = 300.f;
	
	
	
	// Wander steering

	/* How often the wander angle is updated and MoveTo is re-issued (seconds) */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="0.01"))
	float SteeringInterval = 0.1f;

	/* Distance ahead of the agent where the wander circle is projected */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="10.0"))
	float WanderCircleOffset = 150.f;

	/* Radius of the wander circle. Larger = wilder turns */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="10.0"))
	float WanderCircleRadius = 100.f;

	/* Max random change to the wander angle per steering update (radians) */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="0.01"))
	float MaxAngleChange = 0.4f;

	/*
	* How strongly the wander angle is pulled toward the destination bearing
	* each steering update (radians). 0 = pure random, large = nearly straight.
	*/
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="0.0"))
	float DestinationBias = 0.3f;

	/* Acceptance radius passed to MoveTo. */
	UPROPERTY(EditAnywhere, Category="SmartWander", meta=(ClampMin="10.0"))
	float MoveAcceptanceRadius = 50.f;

	
	
	protected:
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory, float DeltaSeconds) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual uint16 GetInstanceMemorySize() const override;

	
	
	private:
	
	FVector PickDestination(APawn* Pawn) const;
};

// Per-instance node memory
struct FSmartWanderMemory
{
	FVector Destination { FVector::ZeroVector };
	float WanderAngle { 0.f };
	float TimeSinceDestination { 0.f };
	float TimeSinceSteering { 0.f };
};