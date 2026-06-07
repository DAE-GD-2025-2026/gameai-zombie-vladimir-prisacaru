// Fill out your copyright notice in the Description page of Project Settings.

#include "BTT_FightBehavior_PrisacaruVladimir.h"
#include "StudentPerceptor_PrisacaruVladimir.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Items/Weapon.h"
#include "Zombies/BaseZombie.h"
#include "Survivor/SurvivorPawn.h"

UBTT_FightBehavior_PrisacaruVladimir::UBTT_FightBehavior_PrisacaruVladimir()
{
	NodeName = TEXT("Fight Behavior");
	bNotifyTick = true;
}

// File-local helpers
namespace
{
	bool HasWeaponWithAmmo(UInventoryComponent* Inv)
	{
		return Inv->GetInventory().ContainsByPredicate([](const ABaseItem* I){
			return IsValid(I) && I->IsA<AWeapon>() && I->GetValue() > 0;
		});
	}

	int32 FindFreeSlot(UInventoryComponent* Inv)
	{
		for (int32 i = 0; i < Inv->GetInventoryCapacity(); ++i)
			if (Inv->GetInventory().IsValidIndex(i) && Inv->GetInventory()[i] == nullptr)
				return i;
		return INDEX_NONE;
	}

	ABaseItem* FindNearestWeapon(UStudentPerceptor_PrisacaruVladimir* P, const FVector& Pos)
	{
		ABaseItem* Best = nullptr; float BestD = MAX_FLT;
		for (ABaseItem* Item : P->GetSeenItems())
		{
			if (!IsValid(Item) || Item->IsHidden()) continue;
			if (Item->GetItemType() != EItemType::Shotgun &&
			    Item->GetItemType() != EItemType::Pistol) continue;
			const float D = FVector::Dist(Pos, Item->GetActorLocation());
			if (D < BestD) { BestD = D; Best = Item; }
		}
		return Best;
	}

	// Returns the position of the nearest valid zombie, or ZeroVector if none
	FVector NearestZombiePos(const TArray<ABaseZombie*>& Zombies, const FVector& PawnPos)
	{
		float    BestDist = MAX_FLT;
		FVector  BestPos  = FVector::ZeroVector;
		for (const ABaseZombie* Z : Zombies)
		{
			if (!IsValid(Z)) continue;
			const float D = FVector::Dist(PawnPos, Z->GetActorLocation());
			if (D < BestDist) { BestDist = D; BestPos = Z->GetActorLocation(); }
		}
		return BestPos;
	}

	void PruneWeapons(UInventoryComponent* Inv)
	{
		const TArray<ABaseItem*>& Items = Inv->GetInventory();
		for (int32 i = 0; i < Items.Num(); ++i)
			if (IsValid(Items[i]) && Items[i]->IsA<AWeapon>() && Items[i]->GetValue() <= 0)
				Inv->RemoveItem(i);
	}
}



EBTNodeResult::Type UBTT_FightBehavior_PrisacaruVladimir::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                     uint8* NodeMemory)
{
	FFightBehaviorMemory* Mem = reinterpret_cast<FFightBehaviorMemory*>(NodeMemory);
	*Mem = FFightBehaviorMemory{};
	Mem->Phase = EFightBehaviorPhase::Idle;

	// Always run during the fight branch
	if (AAIController* Controller = OwnerComp.GetAIOwner())
		if (ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Controller->GetPawn()))
			Survivor->StartRunning();

	return EBTNodeResult::InProgress;
}



void UBTT_FightBehavior_PrisacaruVladimir::TickTask(UBehaviorTreeComponent& OwnerComp,
                                   uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) return;

	APawn* Pawn = Controller->GetPawn();
	if (!Pawn) return;

	ASurvivorPawn* Survivor  = Cast<ASurvivorPawn>(Pawn);
	UInventoryComponent* Inv = Survivor ? Survivor->GetComponentByClass<UInventoryComponent>() : nullptr;
	UStudentPerceptor_PrisacaruVladimir*  Perc = Survivor ? Survivor->GetComponentByClass<UStudentPerceptor_PrisacaruVladimir>()   : nullptr;

	if (!Inv || !Perc) return;

	FFightBehaviorMemory* Mem = reinterpret_cast<FFightBehaviorMemory*>(NodeMemory);
	const FVector PawnPos = Pawn->GetActorLocation();

	// Prune dead items from cache every tick
	Perc->GetSeenItems().RemoveAll([](const ABaseItem* I){ return !IsValid(I) || I->IsHidden(); });

	TArray<ABaseZombie*>& Zombies = Perc->GetSeenZombies();
	Zombies.RemoveAll([](const ABaseZombie* Z){ return !IsValid(Z); });
	Perc->UpdateBlackboardZombieFlag();

	// If all zombies gone, succeed so the BT can exit the combat branch
	if (Zombies.IsEmpty())
	{
		if (Survivor) Survivor->StopRunning();
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	
	// Phase: Engaging
	if (Mem->Phase == EFightBehaviorPhase::Engaging)
	{
		Mem->TimeSinceSteering += DeltaSeconds;
		if (Mem->TimeSinceSteering < SteeringInterval) return;
		Mem->TimeSinceSteering = 0.f;

		// Re-check weapon
		if (Inv) PruneWeapons(Inv);
		if (!HasWeaponWithAmmo(Inv))
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		// Update speed entries and count
		for (ABaseZombie* Z : Zombies)
		{
			if (!IsValid(Z)) continue;
			
			bool bFound = false;
			
			for (FFightSpeedEntry& E : Mem->SpeedEntries)
				if (E.Zombie == Z) { bFound = true; break; }
			
			if (!bFound)
			{
				FFightSpeedEntry E;
				E.Zombie = Z; E.LastPosition = Z->GetActorLocation(); E.SmoothedSpeed = 0.f;
				Mem->SpeedEntries.Add(E);
			}
		}

		int32 AliveCount = 0;
		for (FFightSpeedEntry& E : Mem->SpeedEntries)
		{
			if (!E.Zombie.IsValid()) continue;
			const FVector CurPos = E.Zombie->GetActorLocation();
			const float Raw = FVector::Dist(CurPos, E.LastPosition) / SteeringInterval;
			
			E.SmoothedSpeed = SpeedSmoothingAlpha * E.SmoothedSpeed
				+ (1.f - SpeedSmoothingAlpha) * Raw;
			
			E.LastPosition = CurPos;
			++AliveCount;
		}

		// Too many slow zombies — switch to flee-wander
		const int32 FastCount  = Mem->SpeedEntries.FilterByPredicate([&](const FFightSpeedEntry& E){
			return E.Zombie.IsValid() && E.SmoothedSpeed > FastZombieSpeedThreshold;
		}).Num();
		const int32 SlowCount  = AliveCount - FastCount;

		if (SlowCount > MaxZombiesToFight)
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		// Find nearest zombie and engage
		ABaseZombie* Nearest = nullptr;
		float NearestDist    = MAX_FLT;
		for (ABaseZombie* Z : Zombies)
		{
			if (!IsValid(Z)) continue;
			const float D = FVector::Dist(PawnPos, Z->GetActorLocation());
			if (D < NearestDist) { NearestDist = D; Nearest = Z; }
		}

		if (!Nearest) return;

		if (NearestDist <= AttackRange)
		{
			// Orient towards zombie
			FVector Direction { Nearest->GetActorLocation() - PawnPos };
			Direction.Z = 0.0f;
			Pawn->SetActorRotation(Direction.Rotation());
			
			const TArray<ABaseItem*>& Items = Inv->GetInventory();
			for (int32 i = 0; i < Items.Num(); ++i)
			{
				if (IsValid(Items[i]) && Items[i]->IsA<AWeapon>() && Items[i]->GetValue() > 0)
				{
					Inv->UseItem(i);
					if (Items[i] && Items[i]->GetValue() <= 0) Inv->RemoveItem(i);
					break;
				}
			}
		}
		else
		{
			FAIMoveRequest MR(Nearest->GetActorLocation());
			MR.SetAcceptanceRadius(MoveAcceptanceRadius);
			MR.SetUsePathfinding(true);
			Controller->MoveTo(MR);
		}
		return;
	}
	
	// Phase: WalkingToItem
	if (Mem->Phase == EFightBehaviorPhase::WalkingToItem)
	{
		Mem->PhaseTimer += DeltaSeconds;

		if (!Mem->TargetItem.IsValid() || Mem->TargetItem->IsHidden())
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		if (ItemMoveTimeout > 0.f && Mem->PhaseTimer >= ItemMoveTimeout)
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
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
			Mem->Phase = EFightBehaviorPhase::Idle;
		}
		else if (bDone)
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
		}
		return;
	}
	
	// Phase: WalkingToHouse
	if (Mem->Phase == EFightBehaviorPhase::WalkingToHouse)
	{
		Mem->PhaseTimer += DeltaSeconds;

		if (!Mem->TargetHouse.IsValid())
		{
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		if (HouseMoveTimeout > 0.f && Mem->PhaseTimer >= HouseMoveTimeout)
		{
			Perc->MarkHouseSearched(Mem->TargetHouse.Get());
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		const FVector Center = Mem->TargetHouse->GetBounds().Origin;
		const float D2D = FVector::Dist2D(PawnPos, Center);
		const UPathFollowingComponent* PFC = Controller->GetPathFollowingComponent();
		const bool bDone = PFC && PFC->GetStatus() == EPathFollowingStatus::Idle;

		if (D2D <= HouseArrivalRadius || bDone)
		{
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Spinning;
			Mem->PhaseTimer = 0.f;
			Mem->SpinStartYaw = Pawn->GetActorRotation().Yaw;
		}
		return;
	}
	
	// Phase: Spinning
	if (Mem->Phase == EFightBehaviorPhase::Spinning)
	{
		Mem->PhaseTimer += DeltaSeconds;
		const float Alpha = FMath::Clamp(Mem->PhaseTimer / SpinDuration, 0.f, 1.f);
		Pawn->SetActorRotation(FRotator(0.f, Mem->SpinStartYaw + Alpha * 360.f, 0.f));

		if (Mem->PhaseTimer >= SpinDuration)
		{
			if (Mem->TargetHouse.IsValid())
				Perc->MarkHouseSearched(Mem->TargetHouse.Get());
			
			Mem->TargetHouse.Reset();
			Mem->Phase = EFightBehaviorPhase::Idle;
		}
		
		return;
	}
	
	// Phase: FleeWander
	if (Mem->Phase == EFightBehaviorPhase::FleeWander)
	{
		Mem->TimeSinceWanderDest += DeltaSeconds;
		if (Mem->TimeSinceWanderDest >= WanderDestinationInterval)
		{
			Mem->TimeSinceWanderDest = 0.f;
			Perc->MarkCurrentPositionExplored();
			Controller->StopMovement();
			Mem->Phase = EFightBehaviorPhase::Idle;
			return;
		}

		Mem->TimeSinceWanderSteer += DeltaSeconds;
		if (Mem->TimeSinceWanderSteer >= WanderSteeringInterval)
		{
			Mem->TimeSinceWanderSteer = 0.f;

			const FVector2D Pos2D(PawnPos);
			const FVector2D Fwd2D(Pawn->GetActorForwardVector());

			// Get nearest zombie position this tick (updated live)
			const FVector NearestZombie = NearestZombiePos(Zombies, PawnPos);

			if (!NearestZombie.IsZero())
			{
				// Angle AWAY from nearest zombie — opposite of the direction toward it
				const FVector2D ToZombie  = FVector2D(NearestZombie) - Pos2D;
				const float AwayAngle = FMath::Atan2(-ToZombie.Y, -ToZombie.X);
				const float AngleDiff = FMath::FindDeltaAngleRadians(Mem->WanderAngle, AwayAngle);
				const float BiasStep = FMath::Clamp(ZombieRepulsionBias, 0.f, FMath::Abs(AngleDiff));
				Mem->WanderAngle += FMath::Sign(AngleDiff) * BiasStep;
			}

			Mem->WanderAngle += FMath::FRandRange(-WanderMaxAngleChange, WanderMaxAngleChange);

			const FVector2D CircleCenter = Pos2D + Fwd2D * WanderCircleOffset;
			const FVector2D Target2D = CircleCenter +
				FVector2D(FMath::Cos(Mem->WanderAngle), FMath::Sin(Mem->WanderAngle)) * WanderCircleRadius;

			const FVector MoveTarget(Target2D.X, Target2D.Y, PawnPos.Z);
			if (!Perc->IsTooCloseToPurgeZone(MoveTarget))
			{
				FAIMoveRequest MR(MoveTarget);
				MR.SetAcceptanceRadius(WanderAcceptanceRadius);
				MR.SetUsePathfinding(true);
				Controller->MoveTo(MR);
			}
		}
		
		return;
	}
	
	// Phase: Idle, decide next action
	if (Inv) PruneWeapons(Inv);
	const bool bHasWeapon = HasWeaponWithAmmo(Inv);

	// Has weapon: try to engage
	if (bHasWeapon)
	{
		Mem->Phase = EFightBehaviorPhase::Engaging;
		Mem->TimeSinceSteering = SteeringInterval; // fire immediately
		Mem->SpeedEntries.Reset();
		for (ABaseZombie* Z : Zombies)
		{
			if (!IsValid(Z)) continue;
			FFightSpeedEntry E;
			E.Zombie = Z;
			E.LastPosition = Z->GetActorLocation();
			E.SmoothedSpeed = 0.f;
			Mem->SpeedEntries.Add(E);
		}
		return;
	}

	// No weapon: look for one in the world
	ABaseItem* WeaponItem = FindNearestWeapon(Perc, PawnPos);
	if (WeaponItem && FindFreeSlot(Inv) != INDEX_NONE)
	{
		Mem->TargetItem = WeaponItem;
		Mem->Phase = EFightBehaviorPhase::WalkingToItem;
		Mem->PhaseTimer = 0.f;
		FAIMoveRequest MR(WeaponItem->GetActorLocation());
		MR.SetUsePathfinding(true);
		MR.SetAcceptanceRadius(10.f);
		Controller->MoveTo(MR);
		return;
	}

	// No weapon, no visible weapon: go to nearest unsearched house
	if (Perc->HasUnsearchedHouses())
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
			Mem->Phase = EFightBehaviorPhase::WalkingToHouse;
			Mem->PhaseTimer  = 0.f;
			FAIMoveRequest MR(NearestHouse->GetBounds().Origin);
			MR.SetUsePathfinding(true);
			MR.SetAcceptanceRadius(HouseArrivalRadius);
			Controller->MoveTo(MR);
			return;
		}
	}

	// No weapon, all houses searched: flee-wander away from zombies
	Mem->Phase = EFightBehaviorPhase::FleeWander;
	Mem->TimeSinceWanderDest = 0.f;
	Mem->TimeSinceWanderSteer = WanderSteeringInterval;
	const FVector2D Fwd(Pawn->GetActorForwardVector());
	Mem->WanderAngle = FMath::Atan2(Fwd.Y, Fwd.X);
}

EBTNodeResult::Type UBTT_FightBehavior_PrisacaruVladimir::AbortTask(UBehaviorTreeComponent& OwnerComp,
                                                   uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
		
		if (ASurvivorPawn* Survivor = Cast<ASurvivorPawn>(Controller->GetPawn()))
			Survivor->StopRunning();
	}
	
	return Super::AbortTask(OwnerComp, NodeMemory);
}

uint16 UBTT_FightBehavior_PrisacaruVladimir::GetInstanceMemorySize() const { return sizeof(FFightBehaviorMemory); }