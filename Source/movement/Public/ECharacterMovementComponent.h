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

	class FSavedMove final : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;
	public:
		enum ECompressedFlags
		{
			FLAG_Sprint			= 0x10,
		};
		uint8 bSaved_WantsToSprint : 1;
		uint8 bSaved_PrevWantsToCrunch : 1;
		uint8 bSaved_WantsToProne : 1;

		FSavedMove();
		
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	class FNetworkPredictionData_Client_E final : public FNetworkPredictionData_Client_Character
	{
		typedef FNetworkPredictionData_Client_Character Super;
	public:
		FNetworkPredictionData_Client_E(const UECharacterMovementComponent& Movement);
		virtual FSavedMovePtr AllocateNewMove() override;
	};

	UPROPERTY(EditDefaultsOnly) float MaxSprintSpeed = 600.f;
	UPROPERTY(EditDefaultsOnly) float ProneEnterHoldDuration=0.2f;
	UPROPERTY(EditDefaultsOnly) float MaxProneSpeed=200.f;
	UPROPERTY(EditDefaultsOnly) float BreakingDecelerationProning=2500.f;

	UPROPERTY(Transient) AECharacter* OwningCharacter;
	bool bSafe_WantsToSprint;
	bool bSafe_PrevWantsToCrouch;
	bool bSafe_WantsToProne;
	FTimerHandle TimerHandle_EnterProne;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void TryEnterProne() { bSafe_WantsToProne = true; }
	UFUNCTION(Server, Reliable) void Server_EnterProne();
	
	void EnterProne(EMovementMode PrevMode, ECustomMovementMode PrevCustomMode);
	void ExitProne();
	bool CanProne() const;
	void PhysProne(float deltaTime, int32 Iterations);

public:
	virtual void InitializeComponent() override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	
	UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;
	UFUNCTION(BlueprintPure) bool IsMovementMode(EMovementMode InMovementMode) const;
	
	UFUNCTION(BlueprintCallable) void Sprint();
	UFUNCTION(BlueprintCallable) void StopSprinting();
	UFUNCTION(BlueprintCallable) void EnterCrouch();
	UFUNCTION(BlueprintCallable) void CrouchReleased();
};
