#include "ECharacterMovementComponent.h"
#include "ECharacter.h"

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
	bSaved_WantsToSprint = Movement->bSafe_WantsToSprint;
	bSaved_PrevWantsToCrunch = Movement->bSafe_PrevWantsToCrouch;
	bSaved_WantsToProne = Movement->bSafe_WantsToProne;
}

void UECharacterMovementComponent::FSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	UECharacterMovementComponent* Movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());
	Movement->bSafe_WantsToSprint = bSaved_WantsToSprint;
	Movement->bSafe_PrevWantsToCrouch = bSaved_PrevWantsToCrunch;
	Movement->bSafe_WantsToProne = bSaved_WantsToProne;
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
	bSafe_WantsToProne = true;
}

void UECharacterMovementComponent::EnterProne(EMovementMode PrevMode, ECustomMovementMode PrevCustomMode)
{
	bWantsToCrouch = true;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, true, nullptr);
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
	bSafe_WantsToSprint = (Flags & FSavedMove::FLAG_Sprint) != 0;
}

void UECharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	bSafe_PrevWantsToCrouch = bWantsToCrouch;
}

void UECharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (bSafe_WantsToProne)
	{
		if (CanProne())
		{
			SetMovementMode(MOVE_Custom, CMOVE_Prone);
			if (!CharacterOwner->HasAuthority()) Server_EnterProne();
		}
		bSafe_WantsToProne = false;
	}
	if (IsCustomMovementMode(CMOVE_Prone) && !bWantsToCrouch)
	{
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
	
	if (IsCustomMovementMode(CMOVE_Prone)) EnterProne(PreviousMovementMode, (ECustomMovementMode)PreviousCustomMode);
}

bool UECharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Prone);
}

bool UECharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}

float UECharacterMovementComponent::GetMaxSpeed() const
{
	if (IsMovementMode(MOVE_Walking) && bSafe_WantsToSprint && !IsCrouching()) return MaxSprintSpeed;
	if (!IsMovementMode(MOVE_Custom)) return Super::GetMaxSpeed();
	switch (CustomMovementMode)
	{
	case CMOVE_Prone:
		return MaxProneSpeed;
	default:
		return -1.f;
	}
}

float UECharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (!IsMovementMode(MOVE_Custom)) return Super::GetMaxBrakingDeceleration();
	switch (CustomMovementMode)
	{
	case CMOVE_Prone:
		return BreakingDecelerationProning;
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

void UECharacterMovementComponent::Sprint()
{
	bSafe_WantsToSprint = true;
}

void UECharacterMovementComponent::StopSprinting()
{
	bSafe_WantsToSprint = false;
}

void UECharacterMovementComponent::EnterCrouch()
{
	bWantsToCrouch = !bWantsToCrouch;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_EnterProne, this, &UECharacterMovementComponent::TryEnterProne, ProneEnterHoldDuration);
}

void UECharacterMovementComponent::CrouchReleased()
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_EnterProne);
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
