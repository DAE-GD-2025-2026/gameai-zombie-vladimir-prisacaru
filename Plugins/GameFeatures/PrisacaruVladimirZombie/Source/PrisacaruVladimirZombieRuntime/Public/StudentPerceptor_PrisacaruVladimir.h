// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "Items/BaseItem.h"
#include "Village/House/House.h"
#include "Zombies/BaseZombie.h"
#include "PurgeZones/PurgeZone.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "StudentPerceptor_PrisacaruVladimir.generated.h"

// ---- Blackboard key names ----
namespace BBKeys
{
	static const FName SeesZombies { TEXT("bSeesZombies")   };
	static const FName SeesPurgeZone { TEXT("bSeesPurgeZone") };
	static const FName FleeFromLocation { TEXT("FleeFromLocation") };
	static const FName ShouldSpin { TEXT("bShouldSpin")    };
	static const FName TargetHouse { TEXT("TargetHouse")    };
}

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRISACARUVLADIMIRZOMBIERUNTIME_API UStudentPerceptor_PrisacaruVladimir : public UActorComponent
{
	GENERATED_BODY()

	public:
	
	UStudentPerceptor_PrisacaruVladimir();

	virtual void BeginPlay() override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// Exploration
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Exploration")
	const TArray<FVector>& GetExploredPositions() const { return ExploredPositions; }
	
	UFUNCTION(BlueprintCallable, Category="Exploration")
	void MarkCurrentPositionExplored();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Exploration")
	int32 GetExplorationDensity(const FVector& WorldPos, float Radius) const;

	// Perception caches (tasks read these directly)
	TArray<ABaseItem*>& GetSeenItems() { return SeenItems; }
	TArray<AHouse*>& GetSeenHouses() { return SeenHouses; }
	TArray<ABaseZombie*>& GetSeenZombies() { return SeenZombies; }
	TArray<APurgeZone*>& GetSeenPurgeZones() { return SeenPurgeZones; }

	// BB update helpers (called by tasks and internally)
	void UpdateBlackboardZombieFlag();
	void UpdateBlackboardPurgeFlag();

	// House search tracking
	void MarkHouseSearched(AHouse* House);
	void ResetSearchedHouses();
	bool HasUnsearchedHouses() const;
	bool IsHouseSearched(const AHouse* House) const;

	UBlackboardComponent* GetBlackboard() const { return BB; }

	/* Returns true if WorldPos is within PurgeDangerRadius of any known purge zone */
	bool IsTooCloseToPurgeZone(const FVector& WorldPos) const;

	/* How often (seconds) all house search records are cleared */
	UPROPERTY(EditAnywhere, Category="Exploration", meta=(ClampMin="5.0"))
	float HouseResetInterval = 30.f;

	/* Distance from a purge zone center that triggers fleeing and is rejected by wander */
	UPROPERTY(EditAnywhere, Category="PurgeZone", meta=(ClampMin="50.0"))
	float PurgeDangerRadius = 600.f;

	/* Minimum time (seconds) between damage-triggered spins */
	UPROPERTY(EditAnywhere, Category="Spin", meta=(ClampMin="0.0"))
	float DamageSpinCooldown = 1.f;
	
	

	private:
	
	void TryEnsureBlackboard();
	
	/* Writes FleeFromLocation to the closest threat across both SeenZombies and SeenPurgeZones. */
	void UpdateFleeFromLocation();

	UPROPERTY() UBlackboardComponent* BB { nullptr };

	UPROPERTY() TArray<FVector> ExploredPositions;
	static constexpr float MinRecordDistance = 150.f;

	UPROPERTY() TArray<ABaseItem*> SeenItems;
	UPROPERTY() TArray<AHouse*> SeenHouses;
	UPROPERTY() TSet<AHouse*> SearchedHouses;
	UPROPERTY() TArray<ABaseZombie*> SeenZombies;
	UPROPERTY() TArray<APurgeZone*> SeenPurgeZones;

	float HouseResetAccumulator { 0.f };
	float SpinCooldownAccumulator { 0.f };
	float LastHealth { 0.0f };
};