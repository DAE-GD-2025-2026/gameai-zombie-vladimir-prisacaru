// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_Flee.generated.h"

/**
 * BTT_Flee
 *
 * Flees away from the world position stored in the BB key "FleeFromLocation".
 * Works for both zombie clusters and purge zones — whatever wrote that key last.
 *
 * Each tick:
 *  - Culls invalid/distant zombies AND expired purge zones from the perceptor
 *    arrays, then updates both BB flags.
 *  - If neither flag is set, finishes Succeeded.
 *  - Otherwise projects a flee target FleeDistance away from FleeFromLocation
 *    and issues MoveTo at SteeringInterval.
 */
UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_Flee : public UBTTaskNode
{
	GENERATED_BODY()

	public:
	
	UBTT_Flee();

	/* Distance at which a zombie is no longer considered a threat */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="50.0"))
	float ZombieSafeDistance = 600.f;

	/* Distance at which a purge zone is no longer considered a threat */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="50.0"))
	float PurgeSafeDistance = 800.f;

	/* How far ahead to project the flee target */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="50.0"))
	float FleeDistance = 500.f;

	/* How often (seconds) the flee target is recalculated and MoveTo re-issued */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="0.01"))
	float SteeringInterval = 0.15f;

	/* Acceptance radius passed to MoveTo */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="10.0"))
	float MoveAcceptanceRadius = 50.f;
	
	protected:
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory, float DeltaSeconds) override;
	
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual uint16 GetInstanceMemorySize() const override;
};

struct FFleeMemory
{
	float TimeSinceSteering { 0.f };
};