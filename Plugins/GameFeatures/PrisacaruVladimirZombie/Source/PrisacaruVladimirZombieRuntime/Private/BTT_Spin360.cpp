// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_Spin360.h"
#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Village/House/House.h"

UBTT_Spin360::UBTT_Spin360()
{
	NodeName = TEXT("Spin 360");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTT_Spin360::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) return EBTNodeResult::Failed;

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	// Stop any ongoing movement so the survivor spins in place
	Controller->StopMovement();

	FSpin360Memory* Mem = reinterpret_cast<FSpin360Memory*>(NodeMemory);
	Mem->TimeElapsed = 0.f;
	Mem->StartYaw = Pawn->GetActorRotation().Yaw;

	return EBTNodeResult::InProgress;
}

void UBTT_Spin360::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

	FSpin360Memory* Mem = reinterpret_cast<FSpin360Memory*>(NodeMemory);
	Mem->TimeElapsed += DeltaSeconds;

	// Interpolate yaw from StartYaw to StartYaw + 360 over SpinDuration
	const float Alpha = FMath::Clamp(Mem->TimeElapsed / SpinDuration, 0.f, 1.f);
	const float NewYaw = Mem->StartYaw + Alpha * 360.f;
	Pawn->SetActorRotation(FRotator(0.f, NewYaw, 0.f));

	if (Mem->TimeElapsed >= SpinDuration)
	{
		UStudentPerceptor* P = Pawn->GetComponentByClass<UStudentPerceptor>();
		UBlackboardComponent* BB = P ? P->GetBlackboard() : nullptr;

		if (BB)
		{
			// Clear the spin request flag (set by damage)
			BB->SetValueAsBool(BBKeys::ShouldSpin, false);

			// If the survivor spun because they arrived at a house, mark that house as searched
			if (AHouse* TargetHouse = Cast<AHouse>(BB->GetValueAsObject(BBKeys::TargetHouse)))
			{
				if (P) P->MarkHouseSearched(TargetHouse);

				// Clear the BB key
				BB->SetValueAsObject(BBKeys::TargetHouse, nullptr);
			}
		}

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTT_Spin360::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::AbortTask(OwnerComp, NodeMemory);
}

uint16 UBTT_Spin360::GetInstanceMemorySize() const { return sizeof(FSpin360Memory); }