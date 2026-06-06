// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"


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
