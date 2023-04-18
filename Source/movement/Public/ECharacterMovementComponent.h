// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EMovement.h"

#include "ECharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None    UMETA(Hidden),
	CMOVE_Prone   UMETA(DisplayName = "Prone"),
	CMOVE_MAX     UMETA(Hidden),
};

UCLASS()
class MOVEMENT_API UECharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	class FSavedMove : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;
		uint8 Saved_bWantsToSprint : 1;
		uint8 Saved_bPrevWantsToCrunch : 1;
	public:
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const;
		virtual void Clear();
		virtual uint8 GetCompressedFlags() const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData);
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	class FNetworkPredictionData_Client_E : public FNetworkPredictionData_Client_Character
	{
		typedef FNetworkPredictionData_Client_Character Super;
	public:
		FNetworkPredictionData_Client_E(const UECharacterMovementComponent& Movement);
		virtual FSavedMovePtr AllocateNewMove() override;
	};

	UPROPERTY(EditDefaultsOnly) float Sprint_MaxWalkSpeed;
	UPROPERTY(EditDefaultsOnly) float Walk_MaxWalkSpeed;

	UPROPERTY(Transient) AECharacter* OwningCharacter;

	bool Safe_bWantsToSprint;
	bool Safe_bPrevWantsToCrouch;

protected:
	virtual void InitializeComponent() override;
public:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
public:
	UFUNCTION(BlueprintCallable) void Sprint();
	UFUNCTION(BlueprintCallable) void StopSprinting();
	UFUNCTION(BlueprintCallable) void ToggleCrouch();

	UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;
	UFUNCTION(BlueprintPure) bool IsMovementMode(EMovementMode InMovementMode) const;
};
