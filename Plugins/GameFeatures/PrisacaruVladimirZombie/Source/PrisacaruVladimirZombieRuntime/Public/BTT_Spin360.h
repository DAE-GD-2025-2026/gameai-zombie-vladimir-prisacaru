// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_Spin360.generated.h"

/**
 * BTT_Spin360
 *
 * Rotates the pawn 360 degrees in place over SpinDuration seconds
 * Used after damage (to reveal attackers to AIPerception) and when
 * arriving at an unsearched house (to scan for items inside)
 *
 * On completion, clears the BB key "bShouldSpin"
 */
UCLASS()
class PRISACARUVLADIMIRZOMBIERUNTIME_API UBTT_Spin360 : public UBTTaskNode
{
	GENERATED_BODY()

	public:
	
	UBTT_Spin360();

	/* Total time to complete one full rotation. */
	UPROPERTY(EditAnywhere, Category="Spin360", meta=(ClampMin="0.1"))
	float SpinDuration = 0.2f;

	protected:
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory, float DeltaSeconds) override;
	
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	
	virtual uint16 GetInstanceMemorySize() const override;
};

struct FSpin360Memory
{
	float TimeElapsed { 0.f };
	float StartYaw { 0.f };
};