// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_Flee.generated.h"

/**
 * BTT_Flee
 *
 * Runs away from all zombies currently tracked in UStudentPerceptor::SeenZombies.
 *
 * Each tick:
 *   - Culls zombies whose distance exceeds SafeDistance from the SeenZombies
 *     array, then updates the "bSeesZombies" blackboard key via the perceptor.
 *   - If no zombies remain, finishes Succeeded (BT can switch back to wander).
 *   - Otherwise computes a flee direction (average away-vector from all
 *     remaining zombies), projects a flee target, and calls Controller->MoveTo.
 */
UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_Flee : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTT_Flee();

	/** Distance at which a zombie is considered no longer a threat.
	 *  When all zombies are beyond this, bSeesZombies is cleared and the task succeeds. */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="50.0"))
	float SafeDistance = 600.f;

	/** How far ahead to project the flee target each steering update. */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="50.0"))
	float FleeDistance = 400.f;

	/** How often (seconds) the flee target is recalculated and MoveTo re-issued. */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="0.01"))
	float SteeringInterval = 0.15f;

	/** Acceptance radius passed to MoveTo for the flee step target. */
	UPROPERTY(EditAnywhere, Category="Flee", meta=(ClampMin="10.0"))
	float MoveAcceptanceRadius = 50.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
	                                        uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
	                      uint8* NodeMemory,
	                      float DeltaSeconds) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
	                                      uint8* NodeMemory) override;

	virtual uint16 GetInstanceMemorySize() const override;
};

// Per-instance node memory
struct FFleeMemory
{
	float TimeSinceSteering { 0.f };
};