// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_Flee.h"
#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTT_Flee::UBTT_Flee()
{
	NodeName = TEXT("Flee");
	bNotifyTick = true;
}


EBTNodeResult::Type UBTT_Flee::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                           uint8* NodeMemory)
{
	FFleeMemory* Mem = reinterpret_cast<FFleeMemory*>(NodeMemory);
	
	// Pre-expire so steering fires immediately on the first tick
	Mem->TimeSinceSteering = SteeringInterval;

	return EBTNodeResult::InProgress;
}

void UBTT_Flee::TickTask(UBehaviorTreeComponent& OwnerComp,
                         uint8* NodeMemory,
                         float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	UStudentPerceptor* Perceptor = Pawn->GetComponentByClass<UStudentPerceptor>();
	if (!Perceptor) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	const FVector PawnPos = Pawn->GetActorLocation();

	// Cull zombies that are now beyond SafeDistance
	TArray<ABaseZombie*>& Zombies = Perceptor->GetSeenZombies();
	Zombies.RemoveAll([&](const ABaseZombie* Z)
	{
		return !IsValid(Z) ||
		       FVector::Dist(PawnPos, Z->GetActorLocation()) > SafeDistance;
	});

	// Update the blackboard key now that the list may have changed
	Perceptor->UpdateBlackboardZombieFlag();

	// ---- If no threats remain, we are safe — succeed so BT resumes wander ----
	if (Zombies.IsEmpty())
	{
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// ---- Steering interval: recalculate flee direction and issue MoveTo ----
	FFleeMemory* Mem = reinterpret_cast<FFleeMemory*>(NodeMemory);
	Mem->TimeSinceSteering += DeltaSeconds;

	if (Mem->TimeSinceSteering >= SteeringInterval)
	{
		Mem->TimeSinceSteering = 0.f;

		// Average direction AWAY from all remaining zombies
		FVector FleeDir = FVector::ZeroVector;
		for (const ABaseZombie* Z : Zombies)
		{
			FleeDir += (PawnPos - Z->GetActorLocation());
		}

		// If somehow zero (perfectly centred), fall back to pawn forward
		if (FleeDir.IsNearlyZero())
		{
			FleeDir = Pawn->GetActorForwardVector();
		}
		FleeDir = FleeDir.GetSafeNormal();

		const FVector FleeTarget = PawnPos + FleeDir * FleeDistance;

		FAIMoveRequest MoveReq(FleeTarget);
		MoveReq.SetAcceptanceRadius(MoveAcceptanceRadius);
		MoveReq.SetUsePathfinding(true);
		Controller->MoveTo(MoveReq);
	}
}

// ---------------------------------------------------------------------------

EBTNodeResult::Type UBTT_Flee::AbortTask(UBehaviorTreeComponent& OwnerComp,
                                         uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
		Controller->StopMovement();

	return Super::AbortTask(OwnerComp, NodeMemory);
}

// ---------------------------------------------------------------------------

uint16 UBTT_Flee::GetInstanceMemorySize() const
{
	return sizeof(FFleeMemory);
}