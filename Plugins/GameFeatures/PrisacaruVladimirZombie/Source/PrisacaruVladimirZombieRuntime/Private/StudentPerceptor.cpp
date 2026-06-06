// Fill out your copyright notice in the Description page of Project Settings.

#include <string>

#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"


UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
	}
	
	// Cache blackboard
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			BB = AI->GetBlackboardComponent();
		}
	}
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (ABaseItem* item = Cast<ABaseItem>(Actor);
		item != nullptr && !SeenItems.Contains(item))
	{
		SeenItems.Add(item);
		
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Green, 
	FString::Printf(TEXT("Saw item!")));
	}
	
	if (AHouse* house = Cast<AHouse>(Actor);
		house != nullptr && !SeenHouses.Contains(house))
	{
		SeenHouses.Add(house);
		
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Green, 
	FString::Printf(TEXT("Saw house!")));
	}
	
	if (ABaseZombie* zombie = Cast<ABaseZombie>(Actor);
		zombie != nullptr && !SeenZombies.Contains(zombie))
	{
		SeenZombies.Add(zombie);
		
		UpdateBlackboardZombieFlag();
		
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Red, 
	FString::Printf(TEXT("Saw zombie!")));
	}
}

void UStudentPerceptor::MarkCurrentPositionExplored()
{
	if (!GetOwner()) return;
 
	const FVector CurrentPos = GetOwner()->GetActorLocation();
 
	// Only store if far enough from any already-recorded position
	for (const FVector& Existing : ExploredPositions)
	{
		if (FVector::DistSquared(CurrentPos, Existing) < MinRecordDistance * MinRecordDistance)
		{
			return; // Already covered
		}
	}
 
	ExploredPositions.Add(CurrentPos);
}
 
int32 UStudentPerceptor::GetExplorationDensity(const FVector& WorldPos, float Radius) const
{
	const float RadiusSq = Radius * Radius;
	int32 Count = 0;
 
	for (const FVector& Pos : ExploredPositions)
	{
		if (FVector::DistSquared(WorldPos, Pos) <= RadiusSq)
		{
			++Count;
		}
	}
 
	return Count;
}

void UStudentPerceptor::UpdateBlackboardZombieFlag()
{
	const bool SeesZombies { SeenZombies.Num() > 0 };
	
	GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Red, 
		FString::Printf(TEXT("bSeesZombies set to: %s"),
		SeesZombies ? TEXT("true") : TEXT("false")));
	
	BB->SetValueAsBool(TEXT("bSeesZombies"), SeenZombies.Num() > 0);
}