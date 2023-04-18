#include "ECharacterMovementComponent.h"
#include "ECharacter.h"

#include "GameFramework/Character.h"

bool UECharacterMovementComponent::FSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	FSavedMove* NewEMove = static_cast<FSavedMove*>(NewMove.Get());
	if (Saved_bWantsToSprint != NewEMove->Saved_bWantsToSprint) {
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
	uint8 flags = Super::GetCompressedFlags();
	if (Saved_bWantsToSprint) flags |= FLAG_Custom_0;
	return flags;
}

void UECharacterMovementComponent::FSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	UECharacterMovementComponent* movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());

	Saved_bWantsToSprint = movement->Safe_bWantsToSprint;
	Saved_bPrevWantsToCrunch = movement->Safe_bPrevWantsToCrouch;
}

void UECharacterMovementComponent::FSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	UECharacterMovementComponent* movement = Cast<UECharacterMovementComponent>(C->GetMovementBase());
	movement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	movement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrunch;
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
	Safe_bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UECharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	if (MovementMode == MOVE_Walking) {
		if(Safe_bWantsToSprint) {
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
		UECharacterMovementComponent* self = const_cast<UECharacterMovementComponent*>(this);

		self->ClientPredictionData = new FNetworkPredictionData_Client_E(*this);
		self->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		self->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UECharacterMovementComponent::Sprint()
{
	Safe_bWantsToSprint = true;
}

void UECharacterMovementComponent::StopSprinting()
{
	Safe_bWantsToSprint = false;
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
