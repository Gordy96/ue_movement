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
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UECharacterMovementComponent> ECharacterMovementComponent;
public:
	AECharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	/** Default crouched eye height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float ProneEyeHeight;
	void RecalculateProneEyeHeight();

	/** Set by character movement to specify that this Character is currently crouched. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsProne, Category=Character)
	uint32 bIsProne:1;
	UFUNCTION()
	virtual void OnRep_IsProne();
	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const override;
};
