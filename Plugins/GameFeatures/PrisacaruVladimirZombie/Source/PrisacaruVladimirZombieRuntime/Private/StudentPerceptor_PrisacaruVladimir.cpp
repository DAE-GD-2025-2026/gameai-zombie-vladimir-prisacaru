// Fill out your copyright notice in the Description page of Project Settings.

#include "StudentPerceptor_PrisacaruVladimir.h"
#include "AIController.h"
#include "Survivor/SurvivorPawn.h"

UStudentPerceptor_PrisacaruVladimir::UStudentPerceptor_PrisacaruVladimir()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor_PrisacaruVladimir::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto* Perc = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		Perc->OnTargetPerceptionUpdated.AddDynamic(this,
			&UStudentPerceptor_PrisacaruVladimir::OnPerceptionUpdated);
	}
	
	TryEnsureBlackboard();
}

void UStudentPerceptor_PrisacaruVladimir::TryEnsureBlackboard()
{
	if (BB) return;
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
			BB = AI->GetBlackboardComponent();
}

void UStudentPerceptor_PrisacaruVladimir::TickComponent(float DeltaTime, ELevelTick TickType,
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

	SpinCooldownAccumulator += DeltaTime;

	// Prune purge zones that no longer exist (disappeared), then re-evaluate proximity flag
	SeenPurgeZones.RemoveAll([](const APurgeZone* P){ return !IsValid(P); });
	UpdateBlackboardPurgeFlag();
	
	if (ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(GetOwner()))
	{
		if (UHealthComponent* health = Pawn->GetComponentByClass<UHealthComponent>())
		{
			if (LastHealth > health->GetHealth() && SpinCooldownAccumulator >= DamageSpinCooldown)
			{
				// was damaged and cooldown has elapsed
				if (BB) BB->SetValueAsBool(BBKeys::ShouldSpin, true);
				SpinCooldownAccumulator = 0.f;
				
				GEngine->AddOnScreenDebugMessage(9, 2.f, FColor::Yellow,
					TEXT("Damage: spinning!"));
			}
			
			LastHealth = health->GetHealth();
		}
	}
}


void UStudentPerceptor_PrisacaruVladimir::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
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
		// No immediate flag update here — proximity is evaluated each tick
		GEngine->AddOnScreenDebugMessage(5, 1.f, FColor::Orange, TEXT("Saw purge zone!"));
	}
}

// ---------------------------------------------------------------------------

void UStudentPerceptor_PrisacaruVladimir::MarkCurrentPositionExplored()
{
	if (!GetOwner()) return;
	const FVector Pos = GetOwner()->GetActorLocation();
	for (const FVector& E : ExploredPositions)
		if (FVector::DistSquared(Pos, E) < MinRecordDistance * MinRecordDistance) return;
	ExploredPositions.Add(Pos);
}

int32 UStudentPerceptor_PrisacaruVladimir::GetExplorationDensity(const FVector& WorldPos, float Radius) const
{
	const float R2 = Radius * Radius;
	int32 Count = 0;
	for (const FVector& P : ExploredPositions)
		if (FVector::DistSquared(WorldPos, P) <= R2) ++Count;
	return Count;
}

bool UStudentPerceptor_PrisacaruVladimir::IsTooCloseToPurgeZone(const FVector& WorldPos) const
{
	const float R2 = PurgeDangerRadius * PurgeDangerRadius;
	for (const APurgeZone* P : SeenPurgeZones)
	{
		if (!IsValid(P)) continue;
		if (FVector::DistSquared(WorldPos, P->GetActorLocation()) <= R2) return true;
	}
	return false;
}

void UStudentPerceptor_PrisacaruVladimir::UpdateFleeFromLocation()
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

void UStudentPerceptor_PrisacaruVladimir::UpdateBlackboardZombieFlag()
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

void UStudentPerceptor_PrisacaruVladimir::UpdateBlackboardPurgeFlag()
{
	if (!BB) return;

	// bSeesPurgeZone is true only while the survivor is dangerously close to a known zone
	const FVector PawnPos = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	const bool bInDanger = IsTooCloseToPurgeZone(PawnPos);
	BB->SetValueAsBool(BBKeys::SeesPurgeZone, bInDanger);
	GEngine->AddOnScreenDebugMessage(7, 1.f, FColor::Orange,
		FString::Printf(TEXT("bSeesPurgeZone: %s"), bInDanger ? TEXT("true") : TEXT("false")));

	if (bInDanger)
		UpdateFleeFromLocation();
}

// ---------------------------------------------------------------------------

void UStudentPerceptor_PrisacaruVladimir::MarkHouseSearched(AHouse* House)
{
	if (IsValid(House)) SearchedHouses.Add(House);
}

void UStudentPerceptor_PrisacaruVladimir::ResetSearchedHouses()
{
	SearchedHouses.Empty();
}

bool UStudentPerceptor_PrisacaruVladimir::HasUnsearchedHouses() const
{
	for (const AHouse* H : SeenHouses)
		if (IsValid(H) && !SearchedHouses.Contains(H)) return true;
	return false;
}

bool UStudentPerceptor_PrisacaruVladimir::IsHouseSearched(const AHouse* House) const
{
	return !IsValid(House) || SearchedHouses.Contains(House);
}