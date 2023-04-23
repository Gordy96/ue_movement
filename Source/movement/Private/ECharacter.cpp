#include "ECharacter.h"
#include "ECharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AECharacter::AECharacter(const FObjectInitializer& ObjectInitializer) 
: Super(ObjectInitializer.SetDefaultSubobjectClass<UECharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	ECharacterMovementComponent = Cast<UECharacterMovementComponent>(GetCharacterMovement());
	ECharacterMovementComponent->SetIsReplicated(true);
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AECharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AECharacter::RecalculateProneEyeHeight()
{
	if (ECharacterMovementComponent != nullptr)
	{
		constexpr float EyeHeightRatio = 0.8f;
		CrouchedEyeHeight = ECharacterMovementComponent->ProneHalfHeight * EyeHeightRatio;
	}
}

// Called every frame
void AECharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AECharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AECharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION( AECharacter, bIsProne, COND_SimulatedOnly );
}

void AECharacter::OnRep_IsProne()
{
	if (ECharacterMovementComponent)
	{
		if (bIsProne)
		{
			ECharacterMovementComponent->bWantsToProne = true;
			ECharacterMovementComponent->EnterProne(true);
		}
		else
		{
			ECharacterMovementComponent->bWantsToProne = false;
			ECharacterMovementComponent->EnterProne(true);
		}
		ECharacterMovementComponent->bNetworkUpdateReceived = true;
	}
}
