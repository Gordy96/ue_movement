#include "ECharacter.h"
#include "ECharacterMovementComponent.h"

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

