// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_SafeBehavior.h"
#include "StudentPerceptor.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/HealthComponent.h"
#include "Common/InventoryComponent.h"
#include "Common/StaminaComponent.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Items/Weapon.h"
#include "Survivor/SurvivorPawn.h"

UBTT_SafeBehavior::UBTT_SafeBehavior()
{
	NodeName = TEXT("Safe Behavior");
	bNotifyTick = true;
}

// File-local helpers
namespace
{
	int32 FindUsableSlot(UInventoryComponent* Inv, EItemType Type)
	{
		const TArray<ABaseItem*>& Items = Inv->GetInventory();
		for (int32 i = 0; i < Items.Num(); ++i)
			if (IsValid(Items[i]) && Items[i]->GetItemType() == Type && Items[i]->GetValue() > 0)
				return i;
		return INDEX_NONE;
	}

	bool HasWeaponWithAmmo(UInventoryComponent* Inv)
	{
		return Inv->GetInventory().ContainsByPredicate([](const ABaseItem* I){
			return IsValid(I) && I->IsA<AWeapon>() && I->GetValue() > 0;
		});
	}

	ABaseItem* FindNearestItem(UStudentPerceptor* P, const FVector& Pos,
	                           std::initializer_list<EItemType> Types)
	{
		ABaseItem* Best = nullptr; float BestD = MAX_FLT;
		for (ABaseItem* Item : P->GetSeenItems())
		{
			if (!IsValid(Item) || Item->IsHidden()) continue;
			bool bMatch = false;
			for (EItemType T : Types) if (Item->GetItemType() == T) { bMatch = true; break; }
			if (!bMatch) continue;
			const float D = FVector::Dist(Pos, Item->GetActorLocation());
			if (D < BestD) { BestD = D; Best = Item; }
		}
		return Best;
	}

	int32 FindFreeSlot(UInventoryComponent* Inv)
	{
		for (int32 i = 0; i < Inv->GetInventoryCapacity(); ++i)
			if (Inv->GetInventory().IsValidIndex(i) && Inv->GetInventory()[i] == nullptr)
				return i;
		return INDEX_NONE;
	}

	FVector PickWanderDestination(APawn* Pawn, UStudentPerceptor* P,
	                               float Radius, int32 Candidates, float DensityRadius)
	{
		const FVector Origin = Pawn->GetActorLocation();
		FVector Best = Origin + FVector(Radius, 0, 0);
		int32 LowestDensity = MAX_int32;
		for (int32 i = 0; i < Candidates; ++i)
		{
			const FVector2D Off = FMath::RandPointInCircle(Radius);
			const FVector Cand(Origin.X + Off.X, Origin.Y + Off.Y, Origin.Z);
			const int32 D = P ? P->GetExplorationDensity(Cand, DensityRadius) : 0;
			if (D < LowestDensity) { LowestDensity = D; Best = Cand; if (D == 0) break; }
		}
		return Best;
	}
}



EBTNodeResult::Type UBTT_SafeBehavior::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                    uint8* NodeMemory)
{
	FSafeBehaviorMemory* Mem = reinterpret_cast<FSafeBehaviorMemory*>(NodeMemory);
	*Mem = FSafeBehaviorMemory{}; // fresh state every time task starts

	// Pre-expire timers so we act on the very first tick
	Mem->TimeSinceWanderDest  = WanderDestinationInterval;
	Mem->TimeSinceWanderSteer = WanderSteeringInterval;
	Mem->Phase = ESafeBehaviorPhase::Idle;

	return EBTNodeResult::InProgress;
}



void UBTT_SafeBehavior::TickTask(UBehaviorTreeComponent& OwnerComp,
                                  uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) return;
	
	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) return;

	ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Pawn);
	UInventoryComponent* Inv = Survivor ? Survivor->GetComponentByClass<UInventoryComponent>() : nullptr;
	UHealthComponent* HP = Survivor ? Survivor->GetComponentByClass<UHealthComponent>() : nullptr;
	UStaminaComponent* Stam = Survivor ? Survivor->GetComponentByClass<UStaminaComponent>() : nullptr;
	UStudentPerceptor* Perc = Survivor ? Survivor->GetComponentByClass<UStudentPerceptor>() : nullptr;

	if (!Inv || !Perc) return;

	FSafeBehaviorMemory* Mem = reinterpret_cast<FSafeBehaviorMemory*>(NodeMemory);
	const FVector PawnPos = Pawn->GetActorLocation();
	
	// Phase: WalkingToItem
	if (Mem->Phase == ESafeBehaviorPhase::WalkingToItem)
	{
		Mem->PhaseTimer += DeltaSeconds;

		// Item vanished
		if (!Mem->TargetItem.IsValid() || Mem->TargetItem->IsHidden())
		{
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
			return;
		}

		// Timeout
		if (ItemMoveTimeout > 0.f && Mem->PhaseTimer >= ItemMoveTimeout)
		{
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
			return;
		}

		const float Dist = FVector::Dist(PawnPos, Mem->TargetItem->GetActorLocation());
		const UPathFollowingComponent* PFC = Controller->GetPathFollowingComponent();
		const bool bDone = PFC && PFC->GetStatus() == EPathFollowingStatus::Idle;

		if (Dist <= Inv->GetPickupRange())
		{
			const int32 Slot = FindFreeSlot(Inv);
			if (Slot != INDEX_NONE)
				Inv->GrabItem(Slot, Mem->TargetItem.Get());
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
		}
		else if (bDone)
		{
			// Pathfinder gave up, item unreachable
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
		}
		return;
	}
	
	// Phase: WalkingToHouse
	if (Mem->Phase == ESafeBehaviorPhase::WalkingToHouse)
	{
		Mem->PhaseTimer += DeltaSeconds;

		if (!Mem->TargetHouse.IsValid())
		{
			Mem->Phase = ESafeBehaviorPhase::Idle;
			return;
		}

		if (HouseMoveTimeout > 0.f && Mem->PhaseTimer >= HouseMoveTimeout)
		{
			// Unreachable, mark searched so we don't retry, go idle
			Perc->MarkHouseSearched(Mem->TargetHouse.Get());
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
			return;
		}

		const FVector Center = Mem->TargetHouse->GetBounds().Origin;
		const float D2D = FVector::Dist2D(PawnPos, Center);
		const UPathFollowingComponent* PFC = Controller->GetPathFollowingComponent();
		const bool bDone = PFC && PFC->GetStatus() == EPathFollowingStatus::Idle;

		if (D2D <= HouseArrivalRadius || bDone)
		{
			Controller->StopMovement();
			
			// Write TargetHouse to BB so Spin360 marks it searched on completion
			if (UBlackboardComponent* BB = Perc->GetBlackboard())
				BB->SetValueAsObject(BBKeys::TargetHouse, Mem->TargetHouse.Get());
			
			Mem->Phase = ESafeBehaviorPhase::Spinning;
			Mem->PhaseTimer = 0.f;
			Mem->SpinStartYaw = Pawn->GetActorRotation().Yaw;
		}
		return;
	}
	
	// Phase: Spinning
	if (Mem->Phase == ESafeBehaviorPhase::Spinning)
	{
		Mem->PhaseTimer += DeltaSeconds;
		const float Alpha  = FMath::Clamp(Mem->PhaseTimer / SpinDuration, 0.f, 1.f);
		const float NewYaw = Mem->SpinStartYaw + Alpha * 360.f;
		Pawn->SetActorRotation(FRotator(0.f, NewYaw, 0.f));

		if (Mem->PhaseTimer >= SpinDuration)
		{
			// Mark house as searched, the spin is done so AIPerception has updated
			if (Mem->TargetHouse.IsValid())
				Perc->MarkHouseSearched(Mem->TargetHouse.Get());

			// Also clear the BB TargetHouse so damage-spin doesn't re-mark it
			if (UBlackboardComponent* BB = Perc->GetBlackboard())
				BB->SetValueAsObject(BBKeys::TargetHouse, nullptr);

			// Also clear ShouldSpin in case this spin was damage-triggered
			if (UBlackboardComponent* BB = Perc->GetBlackboard())
				BB->SetValueAsBool(BBKeys::ShouldSpin, false);

			Mem->Phase = ESafeBehaviorPhase::Idle;
		}
		return;
	}
	
	// Phase: Wandering
	if (Mem->Phase == ESafeBehaviorPhase::Wandering)
	{
		// Re-evaluate needs every destination interval, interrupt wander if needed
		Mem->TimeSinceWanderDest += DeltaSeconds;
		if (Mem->TimeSinceWanderDest >= WanderDestinationInterval)
		{
			Mem->TimeSinceWanderDest = 0.f;
			Perc->MarkCurrentPositionExplored();
			
			// Fall through to Idle to re-check needs
			Controller->StopMovement();
			Mem->Phase = ESafeBehaviorPhase::Idle;
			return;
		}

		Mem->TimeSinceWanderSteer += DeltaSeconds;
		if (Mem->TimeSinceWanderSteer >= WanderSteeringInterval)
		{
			Mem->TimeSinceWanderSteer = 0.f;

			const FVector2D Pos2D(PawnPos);
			const FVector2D Fwd2D(Pawn->GetActorForwardVector());

			const FVector2D ToDest = FVector2D(Mem->WanderDestination) - Pos2D;
			const float DestAngle = FMath::Atan2(ToDest.Y, ToDest.X);
			const float AngleDiff = FMath::FindDeltaAngleRadians(Mem->WanderAngle, DestAngle);
			const float BiasStep = FMath::Clamp(WanderDestinationBias, 0.f, FMath::Abs(AngleDiff));
			Mem->WanderAngle += FMath::Sign(AngleDiff) * BiasStep;
			Mem->WanderAngle += FMath::FRandRange(-WanderMaxAngleChange, WanderMaxAngleChange);

			const FVector2D CircleCenter = Pos2D + Fwd2D * WanderCircleOffset;
			const FVector2D Target2D = CircleCenter +
				FVector2D(FMath::Cos(Mem->WanderAngle), FMath::Sin(Mem->WanderAngle)) * WanderCircleRadius;

			FAIMoveRequest MR(FVector(Target2D.X, Target2D.Y, PawnPos.Z));
			MR.SetAcceptanceRadius(WanderAcceptanceRadius);
			MR.SetUsePathfinding(true);
			Controller->MoveTo(MR);
		}
		return;
	}
	
	// Phase: Idle — decide what to do next

	// Prune collected/destroyed items from the cache
	Perc->GetSeenItems().RemoveAll([](const ABaseItem* I){
		return !IsValid(I) || I->IsHidden();
	});

	const float HPFrac = (HP && HP->GetMaxHealth() > 0)
		? static_cast<float>(HP->GetHealth()) / HP->GetMaxHealth() : 1.f;
	
	const float StFrac = (Stam && Stam->GetMaxStamina() > 0)
		? Stam->GetCurrentStamina() / Stam->GetMaxStamina() : 1.f;
	
	const bool bNeedHP = HPFrac < LowHealthThreshold;
	const bool bNeedStam = StFrac < LowStaminaThreshold;
	const bool bNeedWeapon = !HasWeaponWithAmmo(Inv);

	// Priority 1: Use medkit from inventory
	if (bNeedHP)
	{
		const int32 Slot = FindUsableSlot(Inv, EItemType::Medkit);
		if (Slot != INDEX_NONE)
		{
			Inv->UseItem(Slot);
			Inv->RemoveItem(Slot);
			return; // re-evaluate next tick
		}
	}

	// Priority 2: Use food from inventory (for HP or stamina)
	if (bNeedHP || bNeedStam)
	{
		const int32 Slot = FindUsableSlot(Inv, EItemType::Food);
		if (Slot != INDEX_NONE)
		{
			Inv->UseItem(Slot);
			Inv->RemoveItem(Slot);
			return;
		}
	}

	// Priority 3: Walk to a visible item that meets a current need
	if (bNeedHP || bNeedStam || bNeedWeapon)
	{
		ABaseItem* Target = nullptr;
		if (bNeedHP)
		{
			Target = FindNearestItem(Perc, PawnPos, {EItemType::Medkit});
			if (!Target) Target = FindNearestItem(Perc, PawnPos, {EItemType::Food});
		}
		else if (bNeedStam)
		{
			Target = FindNearestItem(Perc, PawnPos, {EItemType::Food});
		}
		if (!Target && bNeedWeapon)
		{
			Target = FindNearestItem(Perc, PawnPos, {EItemType::Shotgun, EItemType::Pistol});
		}

		if (Target && FindFreeSlot(Inv) != INDEX_NONE)
		{
			Mem->TargetItem = Target;
			Mem->Phase = ESafeBehaviorPhase::WalkingToItem;
			Mem->PhaseTimer = 0.f;
			FAIMoveRequest MR(Target->GetActorLocation());
			MR.SetUsePathfinding(true);
			MR.SetAcceptanceRadius(10.f);
			Controller->MoveTo(MR);
			return;
		}
	}

	// Priority 4: Go to an unsearched house (if there's a need)
	if ((bNeedHP || bNeedStam || bNeedWeapon) && Perc->HasUnsearchedHouses())
	{
		AHouse* NearestHouse = nullptr; float NearestDist = MAX_FLT;
		for (AHouse* H : Perc->GetSeenHouses())
		{
			if (!IsValid(H) || Perc->IsHouseSearched(H)) continue;
			const float D = FVector::Dist(PawnPos, H->GetActorLocation());
			if (D < NearestDist) { NearestDist = D; NearestHouse = H; }
		}
		if (NearestHouse)
		{
			Mem->TargetHouse = NearestHouse;
			Mem->Phase = ESafeBehaviorPhase::WalkingToHouse;
			Mem->PhaseTimer = 0.f;
			FAIMoveRequest MR(NearestHouse->GetBounds().Origin);
			MR.SetUsePathfinding(true);
			MR.SetAcceptanceRadius(HouseArrivalRadius);
			Controller->MoveTo(MR);
			return;
		}
	}

	// Priority 5: Wander
	Mem->Phase = ESafeBehaviorPhase::Wandering;
	
	Mem->WanderDestination = PickWanderDestination(Pawn, Perc, WanderRadius,
		WanderCandidates, WanderDensityRadius);
	
	const FVector2D Fwd(Pawn->GetActorForwardVector());
	Mem->WanderAngle = FMath::Atan2(Fwd.Y, Fwd.X);
	Mem->TimeSinceWanderDest = 0.f;
	Mem->TimeSinceWanderSteer = WanderSteeringInterval; // fire on first tick
}



EBTNodeResult::Type UBTT_SafeBehavior::AbortTask(UBehaviorTreeComponent& OwnerComp,
                                                  uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
		Controller->StopMovement();
	
	return Super::AbortTask(OwnerComp, NodeMemory);
}

uint16 UBTT_SafeBehavior::GetInstanceMemorySize() const { return sizeof(FSafeBehaviorMemory); }