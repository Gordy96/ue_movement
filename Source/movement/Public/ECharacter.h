#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EMovement.h"

#include "ECharacter.generated.h"

UCLASS(config=Game)
class MOVEMENT_API AECharacter : public ACharacter
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Movement) UECharacterMovementComponent* ECharacterMovementComponent;
public:
	AECharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
