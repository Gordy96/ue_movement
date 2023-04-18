#include "ECharacterMovementComponent.h"
#include "ECharacter.h"

#include "GameFramework/Character.h"

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
	if (bSaved_WantsToSprint) Flags |= FLAG_Custom_0;
	return Flags;
}

void UECharacterMovementComponent::FSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	const UECharacterMovementComponent* Movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());

	bSaved_WantsToSprint = Movement->bSafe_WantsToSprint;
	bSaved_PrevWantsToCrunch = Movement->bSafe_PrevWantsToCrouch;
}

void UECharacterMovementComponent::FSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	UECharacterMovementComponent* Movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());
	Movement->bSafe_WantsToSprint = bSaved_WantsToSprint;
	Movement->bSafe_PrevWantsToCrouch = bSaved_PrevWantsToCrunch;
}


UECharacterMovementComponent::FNetworkPredictionData_Client_E::FNetworkPredictionData_Client_E(const UECharacterMovementComponent& Movement) : Super(Movement)
{
}

FSavedMovePtr UECharacterMovementComponent::FNetworkPredictionData_Client_E::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove());
}

void UECharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bSafe_WantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UECharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	if (MovementMode == MOVE_Walking) {
		if(bSafe_WantsToSprint) {
			MaxWalkSpeed = Sprint_MaxWalkSpeed;
		}
		else {
			MaxWalkSpeed = Walk_MaxWalkSpeed;
		}
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

void UECharacterMovementComponent::ToggleCrouch()
{
	bWantsToCrouch = !bWantsToCrouch;
}

bool UECharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

bool UECharacterMovementComponent::IsMovementMode(EMovementMode InMovementMode) const
{
	return false;
}

void UECharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	OwningCharacter = Cast<AECharacter>(GetOwner());
}
