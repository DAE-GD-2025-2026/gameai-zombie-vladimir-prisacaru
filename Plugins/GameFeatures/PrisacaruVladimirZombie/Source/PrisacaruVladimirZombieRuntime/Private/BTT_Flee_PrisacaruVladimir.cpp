// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_Flee_PrisacaruVladimir.h"
#include "StudentPerceptor_PrisacaruVladimir.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Survivor/SurvivorPawn.h"



UBTT_Flee_PrisacaruVladimir::UBTT_Flee_PrisacaruVladimir()
{
	NodeName = TEXT("Flee");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_Flee_PrisacaruVladimir::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Switch to running speed while fleeing
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		if (ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Controller->GetPawn()))
			Survivor->StartRunning();
	}

	FFleeMemory* Mem = reinterpret_cast<FFleeMemory*>(NodeMemory);
	Mem->TimeSinceSteering = SteeringInterval; // fire immediately on first tick
	return EBTNodeResult::InProgress;
}

void UBTT_Flee_PrisacaruVladimir::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	UStudentPerceptor_PrisacaruVladimir* Perceptor = Pawn->GetComponentByClass<UStudentPerceptor_PrisacaruVladimir>();
	if (!Perceptor) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	UBlackboardComponent* BB = Perceptor->GetBlackboard();
	if (!BB) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	const FVector PawnPos = Pawn->GetActorLocation();

	// Cull zombies that moved out of range
	TArray<ABaseZombie*>& Zombies = Perceptor->GetSeenZombies();
	Zombies.RemoveAll([&](const ABaseZombie* Z)
	{
		return !IsValid(Z) ||
			FVector::Dist(PawnPos, Z->GetActorLocation()) > ZombieSafeDistance;
	});
	Perceptor->UpdateBlackboardZombieFlag();

	// bSeesPurgeZone is updated by StudentPerceptor::TickComponent each frame — no cull here

	// If no threats remain, we are safe
	const bool bZombies = BB->GetValueAsBool(BBKeys::SeesZombies);
	const bool bPurge = BB->GetValueAsBool(BBKeys::SeesPurgeZone);
	
	if (!bZombies && !bPurge)
	{
		if (ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Pawn))
			Survivor->StopRunning();
		
		Controller->StopMovement();
		
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		
		return;
	}

	// Steering
	FFleeMemory* Mem = reinterpret_cast<FFleeMemory*>(NodeMemory);
	Mem->TimeSinceSteering += DeltaSeconds;
	
	if (Mem->TimeSinceSteering >= SteeringInterval)
	{
		Mem->TimeSinceSteering = 0.f;

		const FVector FleeFrom = BB->GetValueAsVector(BBKeys::FleeFromLocation);
		FVector FleeDir = (PawnPos - FleeFrom);

		if (FleeDir.IsNearlyZero())
			FleeDir = Pawn->GetActorForwardVector();
		
		FleeDir = FleeDir.GetSafeNormal();

		const FVector FleeTarget = PawnPos + FleeDir * FleeDistance;

		FAIMoveRequest MoveReq(FleeTarget);
		MoveReq.SetAcceptanceRadius(MoveAcceptanceRadius);
		MoveReq.SetUsePathfinding(true);
		Controller->MoveTo(MoveReq);
	}
}

EBTNodeResult::Type UBTT_Flee_PrisacaruVladimir::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		if (ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Controller->GetPawn()))
			Survivor->StopRunning();
		
		Controller->StopMovement();
	}
	
	return Super::AbortTask(OwnerComp, NodeMemory);
}

uint16 UBTT_Flee_PrisacaruVladimir::GetInstanceMemorySize() const { return sizeof(FFleeMemory); }