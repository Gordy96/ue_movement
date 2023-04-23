#include "ECharacterMovementComponent.h"
#include "ECharacter.h"
#include "Components/CapsuleComponent.h"

#include "GameFramework/Character.h"

UECharacterMovementComponent::FSavedMove::FSavedMove()
{
	bSaved_WantsToSprint = 1;
	bSaved_PrevWantsToCrunch = 1;
}

bool UECharacterMovementComponent::FSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove* EMove = static_cast<FSavedMove*>(NewMove.Get());
	if (bSaved_WantsToSprint != EMove->bSaved_WantsToSprint) {
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UECharacterMovementComponent::FSavedMove::Clear()
{
	Super::Clear();
}

uint8 UECharacterMovementComponent::FSavedMove::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSaved_WantsToSprint) Flags |= FLAG_Sprint;
	return Flags;
}

void UECharacterMovementComponent::FSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	const UECharacterMovementComponent* Movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());
	bSaved_WantsToSprint = Movement->bWantsToSprint;
	bSaved_PrevWantsToCrunch = Movement->bPrevWantsToCrouch;
	bSaved_WantsToProne = Movement->bWantsToProne;
}

void UECharacterMovementComponent::FSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	UECharacterMovementComponent* Movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());
	Movement->bWantsToSprint = bSaved_WantsToSprint;
	Movement->bPrevWantsToCrouch = bSaved_PrevWantsToCrunch;
	Movement->bWantsToProne = bSaved_WantsToProne;
}


UECharacterMovementComponent::FNetworkPredictionData_Client_E::FNetworkPredictionData_Client_E(const UECharacterMovementComponent& Movement) : Super(Movement)
{
}

FSavedMovePtr UECharacterMovementComponent::FNetworkPredictionData_Client_E::AllocateNewMove()
{
	return MakeShared<FSavedMove>();
}

void UECharacterMovementComponent::Server_EnterProne_Implementation()
{
	bWantsToProne = true;
}

void UECharacterMovementComponent::EnterProne(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	if (!bClientSimulation && !CanCrouchInCurrentState())
	{
		return;
	}
	UCapsuleComponent* Capsule = OwningCharacter->GetCapsuleComponent();
	// See if collision is already at desired size.
	if (Capsule->GetUnscaledCapsuleHalfHeight() == ProneHalfHeight)
	{
		if (!bClientSimulation)
		{
			OwningCharacter->bIsProne = true;
			OwningCharacter->bIsCrouched = false;
		}
		//TODO: update with OnStartProne
		OwningCharacter->OnStartCrouch( 0.f, 0.f );
		return;
	}

	if (bClientSimulation && OwningCharacter->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// restore collision size before going prone
		ACharacter* DefaultCharacter = OwningCharacter->GetClass()->GetDefaultObject<ACharacter>();
		Capsule->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}

	// Change collision size to prone dimensions
	const float ComponentScale = Capsule->GetShapeScale();
	const float OldUnscaledHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = Capsule->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedProneHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, ProneHalfHeight);
	Capsule->SetCapsuleSize(OldUnscaledRadius, ClampedProneHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedProneHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	if( !bClientSimulation )
	{
		// Prone to a larger height? (this is rare)
		if (ClampedProneHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(ProneTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bProne = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,ScaledHalfHeightAdjust), FQuat::Identity,
				UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);
			// If prone, cancel
			if( bProne )
			{
				Capsule->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (bCrouchMaintainsBaseLocation)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(FVector(0.f, 0.f, -ScaledHalfHeightAdjust), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		OwningCharacter->bIsProne = true;
		OwningCharacter->bIsCrouched = false;
	}

	bForceNextFloorCheck = true;

	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedProneHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	AdjustProxyCapsuleSize();
	//TODO: update with OnStartProne
	CharacterOwner->OnStartCrouch( HalfHeightAdjust, ScaledHalfHeightAdjust );

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset -= FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

void UECharacterMovementComponent::ExitProne()
{
}

bool UECharacterMovementComponent::CanProne() const
{
	return IsWalking() || IsCrouching();
}

void UECharacterMovementComponent::PhysProne(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}
	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	while ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent * const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != nullptr) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;
		Acceleration.Z = 0.f;

		if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
		}
		
		ApplyRootMotionToVelocity(timeTick);
		
		if( IsFalling() )
		{
			StartNewPhysics(remainingTime+timeTick, Iterations-1);
			return;
		}
		
		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity; // dx = v * dt
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if ( IsFalling() )
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));
				}
				StartNewPhysics(remainingTime,Iterations);
				return;
			}
			else if ( IsSwimming() ) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}
		
		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if ( bCheckLedges && !CurrentFloor.IsWalkableFloor() )
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f,0.f,-1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
			if ( !NewDelta.IsZero() )
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta/timeTick; // v = dx/dt
				remainingTime += timeTick;
				continue;
			}
			else
			{
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ( (bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if ( IsSwimming() )
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;
			}
		}
		
		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround())
		{
			// Make velocity reflect actual move
			if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; // v = dx / dt
				MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

void UECharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove::FLAG_Sprint) != 0;
}

void UECharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	bPrevWantsToCrouch = bWantsToCrouch;
}

void UECharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (bWantsToProne && !IsCustomMovementMode(CMOVE_Prone) && CanProne())
	{
		SetMovementMode(MOVE_Custom, CMOVE_Prone);
		if (!CharacterOwner->HasAuthority())
		{
			Server_EnterProne();
		}
	}
	if (IsCustomMovementMode(CMOVE_Prone) && !bWantsToProne)
	{
		bWantsToProne = false;
		SetMovementMode(MOVE_Walking);
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UECharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);
	switch (CustomMovementMode)
	{
	case CMOVE_Prone:
		PhysProne(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid movement mode"))
	}
}

void UECharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Prone) ExitProne();
	
	if (IsCustomMovementMode(CMOVE_Prone)) EnterProne(false);
}

bool UECharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Prone);
}

float UECharacterMovementComponent::GetMaxSpeed() const
{
	switch(MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		if (IsCrouching())
		{
			return MaxWalkSpeedCrouched;
		}
		if (bWantsToSprint)
		{
			return MaxSprintSpeed;
		}
		return MaxWalkSpeed;
	case MOVE_Falling:
		return MaxWalkSpeed;
	case MOVE_Swimming:
		return MaxSwimSpeed;
	case MOVE_Flying:
		return MaxFlySpeed;
	case MOVE_Custom:
		switch (CustomMovementMode)
		{
		case CMOVE_Prone:
			return MaxProneSpeed;
		default:
			return MaxCustomMovementSpeed;
		}
	case MOVE_None:
	default:
		return Super::GetMaxSpeed();
	}
}

float UECharacterMovementComponent::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration();
}

float UECharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (!IsMovementMode(MOVE_Custom)) return Super::GetMaxBrakingDeceleration();
	switch (CustomMovementMode)
	{
	case CMOVE_Prone:
		return BreakingDecelerationProne;
	default:
		return -1.f;
	}
}

FNetworkPredictionData_Client* UECharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)
	if (ClientPredictionData == nullptr) {
		UECharacterMovementComponent* Self = const_cast<UECharacterMovementComponent*>(this);

		Self->ClientPredictionData = new FNetworkPredictionData_Client_E(*this);
		Self->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		Self->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UECharacterMovementComponent::SprintPressed()
{
	bWantsToSprint = true;
}

void UECharacterMovementComponent::SprintReleased()
{
	bWantsToSprint = false;
}

void UECharacterMovementComponent::CrouchPressed()
{
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_EnterProne, this, &UECharacterMovementComponent::TryEnterProne, ProneEnterHoldDuration);
}

void UECharacterMovementComponent::CrouchReleased()
{
	if (!IsCustomMovementMode(CMOVE_Prone))
	{
		bWantsToCrouch = !bWantsToCrouch;
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_EnterProne);
	}
}

bool UECharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

bool UECharacterMovementComponent::IsMovementMode(EMovementMode InMovementMode) const
{
	return MovementMode == InMovementMode;
}

void UECharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	OwningCharacter = Cast<AECharacter>(GetOwner());
}
