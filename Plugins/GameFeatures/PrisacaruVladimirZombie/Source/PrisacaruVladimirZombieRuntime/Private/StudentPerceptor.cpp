// Fill out your copyright notice in the Description page of Project Settings.

#include "StudentPerceptor.h"
#include "AIController.h"
#include "Survivor/SurvivorPawn.h"

UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto* Perc = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		Perc->OnTargetPerceptionUpdated.AddDynamic(this,
			&UStudentPerceptor::OnPerceptionUpdated);
	}
	
	TryEnsureBlackboard();
}

void UStudentPerceptor::TryEnsureBlackboard()
{
	if (BB) return;
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
			BB = AI->GetBlackboardComponent();
}

void UStudentPerceptor::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TryEnsureBlackboard();

	HouseResetAccumulator += DeltaTime;
	if (HouseResetAccumulator >= HouseResetInterval)
	{
		HouseResetAccumulator = 0.f;
		SearchedHouses.Empty();
		GEngine->AddOnScreenDebugMessage(8, 2.f, FColor::Cyan, TEXT("Houses reset."));
	}
	
	if (ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(GetOwner()))
	{
		if (UHealthComponent* health = Pawn->GetComponentByClass<UHealthComponent>())
		{
			if (LastHealth > health->GetHealth())
			{
				// was damaged
				if (BB) BB->SetValueAsBool(BBKeys::ShouldSpin, true);
				
				GEngine->AddOnScreenDebugMessage(9, 2.f, FColor::Yellow,
					TEXT("Damage: spinning!"));
			}
			
			LastHealth = health->GetHealth();
		}
	}
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	TryEnsureBlackboard();

	if (ABaseItem* Item = Cast<ABaseItem>(Actor); Item && !SeenItems.Contains(Item))
	{
		SeenItems.Add(Item);
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Green, TEXT("Saw item!"));
	}

	if (AHouse* House = Cast<AHouse>(Actor); House && !SeenHouses.Contains(House))
	{
		SeenHouses.Add(House);
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Green, TEXT("Saw house!"));
	}

	if (ABaseZombie* Zombie = Cast<ABaseZombie>(Actor); Zombie && !SeenZombies.Contains(Zombie))
	{
		SeenZombies.Add(Zombie);
		UpdateBlackboardZombieFlag();
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Red, TEXT("Saw zombie!"));
	}

	if (APurgeZone* Purge = Cast<APurgeZone>(Actor); Purge && !SeenPurgeZones.Contains(Purge))
	{
		SeenPurgeZones.Add(Purge);
		UpdateBlackboardPurgeFlag();
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Orange, TEXT("Saw purge zone!"));
	}
}

// ---------------------------------------------------------------------------

void UStudentPerceptor::MarkCurrentPositionExplored()
{
	if (!GetOwner()) return;
	const FVector Pos = GetOwner()->GetActorLocation();
	for (const FVector& E : ExploredPositions)
		if (FVector::DistSquared(Pos, E) < MinRecordDistance * MinRecordDistance) return;
	ExploredPositions.Add(Pos);
}

int32 UStudentPerceptor::GetExplorationDensity(const FVector& WorldPos, float Radius) const
{
	const float R2 = Radius * Radius;
	int32 Count = 0;
	for (const FVector& P : ExploredPositions)
		if (FVector::DistSquared(WorldPos, P) <= R2) ++Count;
	return Count;
}

// ---------------------------------------------------------------------------

void UStudentPerceptor::UpdateFleeFromLocation()
{
	if (!BB) return;

	const FVector PawnPos = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	FVector BestPos = FVector::ZeroVector;
	float BestDist = MAX_FLT;

	for (const ABaseZombie* Z : SeenZombies)
	{
		if (!IsValid(Z)) continue;
		const float D = FVector::Dist(PawnPos, Z->GetActorLocation());
		if (D < BestDist) { BestDist = D; BestPos = Z->GetActorLocation(); }
	}

	for (const APurgeZone* P : SeenPurgeZones)
	{
		if (!IsValid(P)) continue;
		const float D = FVector::Dist(PawnPos, P->GetActorLocation());
		if (D < BestDist) { BestDist = D; BestPos = P->GetActorLocation(); }
	}

	if (BestDist < MAX_FLT)
		BB->SetValueAsVector(BBKeys::FleeFromLocation, BestPos);
}

void UStudentPerceptor::UpdateBlackboardZombieFlag()
{
	if (!BB) return;

	// Prune dead/invalid zombies first
	SeenZombies.RemoveAll([](const ABaseZombie* Z){ return !IsValid(Z); });

	const bool bSees = SeenZombies.Num() > 0;
	BB->SetValueAsBool(BBKeys::SeesZombies, bSees);
	GEngine->AddOnScreenDebugMessage(6, 1.f, FColor::Red,
		FString::Printf(TEXT("bSeesZombies: %s"), bSees ? TEXT("true") : TEXT("false")));

	UpdateFleeFromLocation();
}

void UStudentPerceptor::UpdateBlackboardPurgeFlag()
{
	if (!BB) return;

	SeenPurgeZones.RemoveAll([](const APurgeZone* P){ return !IsValid(P); });

	const bool bSees = SeenPurgeZones.Num() > 0;
	BB->SetValueAsBool(BBKeys::SeesPurgeZone, bSees);
	GEngine->AddOnScreenDebugMessage(7, 1.f, FColor::Orange,
		FString::Printf(TEXT("bSeesPurgeZone: %s"), bSees ? TEXT("true") : TEXT("false")));

	UpdateFleeFromLocation();
}

// ---------------------------------------------------------------------------

void UStudentPerceptor::MarkHouseSearched(AHouse* House)
{
	if (IsValid(House)) SearchedHouses.Add(House);
}

void UStudentPerceptor::ResetSearchedHouses()
{
	SearchedHouses.Empty();
}

bool UStudentPerceptor::HasUnsearchedHouses() const
{
	for (const AHouse* H : SeenHouses)
		if (IsValid(H) && !SearchedHouses.Contains(H)) return true;
	return false;
}

bool UStudentPerceptor::IsHouseSearched(const AHouse* House) const
{
	return !IsValid(House) || SearchedHouses.Contains(House);
}