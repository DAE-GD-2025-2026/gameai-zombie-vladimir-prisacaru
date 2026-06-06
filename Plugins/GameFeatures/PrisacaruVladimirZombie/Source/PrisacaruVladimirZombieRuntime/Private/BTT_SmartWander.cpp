// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_SmartWander.h"
#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTT_SmartWander::UBTT_SmartWander()
{
	NodeName = TEXT("Smart Wander");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_SmartWander::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                  uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) return EBTNodeResult::Failed;

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	FSmartWanderMemory* Mem = reinterpret_cast<FSmartWanderMemory*>(NodeMemory);

	// Seed wander angle from forward direction to avoid a snap on frame 1
	const FVector2D Fwd(Pawn->GetActorForwardVector());
	Mem->WanderAngle = FMath::Atan2(Fwd.Y, Fwd.X);

	// Pick an initial destination and force both intervals to fire on the
	// first tick by pre-expiring their timers
	Mem->Destination = PickDestination(Pawn);
	Mem->TimeSinceDestination = DestinationInterval;
	Mem->TimeSinceSteering = SteeringInterval;

	return EBTNodeResult::InProgress;
}

void UBTT_SmartWander::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	FSmartWanderMemory* Mem = reinterpret_cast<FSmartWanderMemory*>(NodeMemory);

	// Destination interval, pick a new unexplored target
	Mem->TimeSinceDestination += DeltaSeconds;
	if (Mem->TimeSinceDestination >= DestinationInterval)
	{
		Mem->Destination = PickDestination(Pawn);
		Mem->TimeSinceDestination = 0.f;

		if (UStudentPerceptor* P = Pawn->GetComponentByClass<UStudentPerceptor>())
			P->MarkCurrentPositionExplored();
	}

	// Steering interval, update wander angle and call MoveTo
	Mem->TimeSinceSteering += DeltaSeconds;
	
	if (Mem->TimeSinceSteering >= SteeringInterval)
	{
		Mem->TimeSinceSteering = 0.f;

		const FVector PawnPos = Pawn->GetActorLocation();
		const FVector2D Pos2D(PawnPos);
		const FVector2D Fwd2D(Pawn->GetActorForwardVector());

		// Pull wander angle toward destination
		const FVector2D ToDestination = FVector2D(Mem->Destination) - Pos2D;
		const float DestAngle = FMath::Atan2(ToDestination.Y, ToDestination.X);
		const float AngleDiff = FMath::FindDeltaAngleRadians(Mem->WanderAngle, DestAngle);
		const float BiasStep = FMath::Clamp(DestinationBias, 0.f, FMath::Abs(AngleDiff));
		Mem->WanderAngle += FMath::Sign(AngleDiff) * BiasStep;

		// Random perturbation
		Mem->WanderAngle += FMath::FRandRange(-MaxAngleChange, MaxAngleChange);

		// Wander circle: project ahead, pick point on edge
		const FVector2D CircleCenter = Pos2D + Fwd2D * WanderCircleOffset;
		const FVector2D Target2D = CircleCenter
			+ FVector2D(FMath::Cos(Mem->WanderAngle), FMath::Sin(Mem->WanderAngle))
				* WanderCircleRadius;

		const FVector MoveTarget(Target2D.X, Target2D.Y, PawnPos.Z);

		FAIMoveRequest MoveReq(MoveTarget);
		MoveReq.SetAcceptanceRadius(MoveAcceptanceRadius);
		MoveReq.SetUsePathfinding(true);
		Controller->MoveTo(MoveReq);
	}
}

EBTNodeResult::Type UBTT_SmartWander::AbortTask(UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
		Controller->StopMovement();

	return Super::AbortTask(OwnerComp, NodeMemory);
}

uint16 UBTT_SmartWander::GetInstanceMemorySize() const
{
	return sizeof(FSmartWanderMemory);
}

FVector UBTT_SmartWander::PickDestination(APawn* Pawn) const
{
	UStudentPerceptor* Perceptor = Pawn->GetComponentByClass<UStudentPerceptor>();
	const FVector Origin = Pawn->GetActorLocation();

	FVector BestPoint = Origin + FVector(WanderRadius, 0.f, 0.f);
	int32 LowestDensity = MAX_int32;

	for (int32 i = 0; i < NumCandidates; ++i)
	{
		const FVector2D Offset2D = FMath::RandPointInCircle(WanderRadius);
		const FVector Candidate(Origin.X + Offset2D.X, Origin.Y + Offset2D.Y, Origin.Z);

		const int32 Density = Perceptor
			? Perceptor->GetExplorationDensity(Candidate, DensityQueryRadius)
			: 0;

		if (Density < LowestDensity)
		{
			LowestDensity = Density;
			BestPoint = Candidate;
			if (Density == 0) break;
		}
	}

	return BestPoint;
}