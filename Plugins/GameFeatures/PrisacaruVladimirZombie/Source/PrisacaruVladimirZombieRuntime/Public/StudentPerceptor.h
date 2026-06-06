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
#include "StudentPerceptor.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRISACARUVLADIMIRZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptor();
	
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
	// -------------------------------------------------------
	// Exploration data – used by BTT_SmartWander
	// -------------------------------------------------------
 
	/**
	 * Returns all world-space positions where the survivor has already
	 * observed something via AIPerception.  These represent "explored" spots.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Exploration")
	const TArray<FVector>& GetExploredPositions() const { return ExploredPositions; }
 
	/**
	 * Registers the owner's current location as explored.
	 * Called by BTT_SmartWander periodically while the survivor is moving
	 * and on arrival at each waypoint, building a trail of visited positions.
	 */
	UFUNCTION(BlueprintCallable, Category="Exploration")
	void MarkCurrentPositionExplored();
 
	/**
	 * Returns how many times the region around WorldPos (within Radius) has
	 * been marked explored.  Higher = better known area.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Exploration")
	int32 GetExplorationDensity(const FVector& WorldPos, float Radius) const;
	
	/* Returns an array of all items seen by the Agent */
	UFUNCTION(BlueprintCallable, Category="Cache")
	TArray<ABaseItem*>& GetSeenItems() { return SeenItems; };
	
	/* Returns an array of all houses seen by the Agent */
	UFUNCTION(BlueprintCallable, Category="Cache")
	TArray<AHouse*>& GetSeenHouses() { return SeenHouses; };
	
	/* Returns an array of all zombies seen by the Agent */
	UFUNCTION(BlueprintCallable, Category="Cache")
	TArray<ABaseZombie*>& GetSeenZombies() { return SeenZombies; };
	
	private:
	
	// All world positions that have been "seen" by the survivor
	UPROPERTY()
	TArray<FVector> ExploredPositions;
 
	// Minimum distance between stored positions to avoid redundant entries
	static constexpr float MinRecordDistance = 150.f;
	
	// All items seen by the Agent
	UPROPERTY()
	TArray<ABaseItem*> SeenItems;
	
	// All houses seen by the Agent
	UPROPERTY()
	TArray<AHouse*> SeenHouses;
	
	// All zombies seen by the Agent
	UPROPERTY()
	TArray<ABaseZombie*> SeenZombies;
};
