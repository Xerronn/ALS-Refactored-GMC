#pragma once

#include "GMCOrganicMovementComponent.h"
#include "Settings/AlsCharacterSettings.h"
#include "Settings/AlsMovementSettings.h"
#include "State/AlsLocomotionState.h"
#include "State/AlsMantlingState.h"
#include "State/AlsRagdollingState.h"
#include "State/AlsRollingState.h"
#include "State/AlsViewState.h"
#include "AlsCharacterMovementComponent.generated.h"

using FAlsPhysicsRotationDelegate = TMulticastDelegate<void(float DeltaTime)>;

UCLASS(ClassGroup = "ALS")
class ALS_API UAlsCharacterMovementComponent : public UGMC_OrganicMovementCmp
{
	friend class AAlsCharacter;
	GENERATED_BODY()

public:
	// If checked, this improves the response to interaction from moving kinematic physical
	// bodies, but may cause some issues when interacting with simulated physical bodies.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings", Transient)
	uint8 bAllowImprovedPenetrationAdjustment : 1 {true};

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<AAlsCharacter> CharacterOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GMC/ALS")
	float StandingHalfHeight{0.f};
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GMC/ALS")
	float DefaultRadius{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GMC/ALS")
	float CrouchedHalfHeight{60.f};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GMC/ALS")
	float ProneHalfHeight{30.f};

	//settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GMC/ALS")
	float ChangeStanceSpeed{100.f};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GMC/ALS")
	float JumpForce{500.f};
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	FAlsMovementGaitSettings GaitSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	bool bCanJump{false};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	FGameplayTag MaxAllowedGait{AlsGaitTags::Running};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient, Meta = (ClampMin = 0, ClampMax = 3))
	float GaitAmount{0.0f};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	uint8 bMovementModeLocked : 1 {false};

	// Used to temporarily prohibit the player from moving the character. Also works for AI-controlled characters.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	uint8 bInputBlocked : 1 {false};

	// Valid only on locally controlled characters.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	FRotator PreviousControlRotation{ForceInit};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	FVector PendingPenetrationAdjustment{ForceInit};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	FVector PrePenetrationAdjustmentVelocity{ForceInit};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	uint8 bPrePenetrationAdjustmentVelocityValid : 1 {false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsCharacterSettings> Settings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsMovementSettings> MovementSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	bool bDesiredAiming{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	bool bDesiredJumping{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State|Als Character")
	bool bJustJumped{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	bool bDesiredRagdolling{false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	FGameplayTag DesiredRotationMode{AlsRotationModeTags::ViewDirection};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	FGameplayTag DesiredStance{AlsStanceTags::Standing};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	FGameplayTag DesiredGait{AlsGaitTags::Running};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	FGameplayTag ViewMode{AlsViewModeTags::ThirdPerson};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State")
	FGameplayTag OverlayMode{AlsOverlayModeTags::Default};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag RotationMode{AlsRotationModeTags::ViewDirection};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Stance{AlsStanceTags::Standing};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Gait{AlsGaitTags::Walking};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag LocomotionAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsViewState ViewState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FVector_NetQuantizeNormal InputDirection{ForceInit};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsLocomotionState LocomotionState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsMantlingState MantlingState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FVector_NetQuantize RagdollTargetLocation{ForceInit};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRagdollingState RagdollingState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRollingState RollingState;

	FTimerHandle BrakingFrictionFactorResetTimer;

	//start of input handling
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SprintAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> WalkAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> RotationModeAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ViewModeAction{nullptr};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SwitchShoulderAction{nullptr};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> AimAction{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> RagdollAction{nullptr};
	

public:
	FAlsPhysicsRotationDelegate OnPhysicsRotation;

public:
	UAlsCharacterMovementComponent();

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void BeginPlay() override;

	//GMC functions
protected:
	void OnMovementModeChanged_Implementation(EGMC_MovementMode PreviousMovementMode) override;
	
	void BindReplicationData_Implementation() override;
	
	void SetupPlayerInputComponent_Implementation(UInputComponent* PlayerInputComponent) override;
	
	FVector PreProcessInputVector_Implementation(FVector InRawInputVector) override;
	
	void ClampToValidValues() override;
	
	void PreMovementUpdate_Implementation(float DeltaSeconds) override;
	
	void MovementUpdate_Implementation(float DeltaSeconds) override;
	
	void MovementUpdateSimulated_Implementation(float DeltaSeconds) override;
	
	FVector TransformInputVectorAbsoluteZ(const FVector& AbsoluteInputVector) const override;
private:
	EGMC_CollisionShape InterpToSphereAndSwitchCollisionShape(EGMC_CollisionShape CurrentShape, float SphereRadius, float DeltaSeconds);
	void MaintainMeshOffset();
	void MaintainMeshOffsetSimulated();
	//End of GMC functions
	
	//Start of input actions
protected:
	virtual void StartSprintAction(const FInputActionInstance& InputAction);
	virtual void StopSprintAction(const FInputActionInstance& InputAction);

	virtual void StartWalkAction(const FInputActionInstance& InputAction);
	virtual void StopWalkAction(const FInputActionInstance& InputAction);

	virtual void StartCrouchAction(const FInputActionInstance& InputAction);
	virtual void StopCrouchAction(const FInputActionInstance& InputAction);

	virtual void StartJumpAction(const FInputActionInstance& InputAction);
	virtual void StopJumpAction(const FInputActionInstance& InputAction);

	virtual void StartRotationModeAction(const FInputActionInstance& InputAction);
	virtual void StopRotationModeAction(const FInputActionInstance& InputAction);

	virtual void StartViewModeAction(const FInputActionInstance& InputAction);
	virtual void StopViewModeAction(const FInputActionInstance& InputAction);

	virtual void StartSwitchShoulderAction(const FInputActionInstance& InputAction);
	virtual void StopSwitchShoulderAction(const FInputActionInstance& InputAction);

	virtual void StartAimAction(const FInputActionInstance& InputAction);
	virtual void StopAimAction(const FInputActionInstance& InputAction);

	virtual void StartRagdollAction(const FInputActionInstance& InputAction);
	virtual void StopRagdollAction(const FInputActionInstance& InputAction);
	//end of input actions

public:
	UFUNCTION(BlueprintCallable, Category = "ALS|Character Movement")
	void SetMovementSettings(UAlsMovementSettings* NewMovementSettings);

	const FAlsMovementGaitSettings& GetGaitSettings() const;

	// Returns the character's current speed, mapped to the speed ranges from the movement settings.
	// Varies from 0 to 3, where 0 is stopped, 1 is walking, 2 is running, and 3 is sprinting.
	float GetGaitAmount() const;

private:
	void RefreshGaitSettings();

public:
	void SetMaxAllowedGait(const FGameplayTag& NewMaxAllowedGait);
	
	const FGameplayTag& GetMaxAllowedGait() const;

private:
	void RefreshGroundedMovementSettings();

public:
	void SetMovementModeLocked(bool bNewMovementModeLocked);

	void SetInputBlocked(bool bNewInputBlocked);

	bool TryConsumePrePenetrationAdjustmentVelocity(FVector& OutVelocity);
	
	// View Mode

public:
	const FGameplayTag& GetViewMode() const;

private:
	void SetViewMode(const FGameplayTag& NewViewMode);
	
	// Desired Aiming

public:
	bool IsDesiredAiming() const;

private:
	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
	void SetDesiredAiming(bool bNewDesiredAiming);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnDesiredAimingChanged(bool bPreviousDesiredAiming);

	// Desired Rotation Mode

public:
	const FGameplayTag& GetDesiredRotationMode() const;

private:
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredRotationMode"))
	void SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode);

	// Rotation Mode

public:
	const FGameplayTag& GetRotationMode() const;

protected:
	void SetRotationMode(const FGameplayTag& NewRotationMode);

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRotationModeChanged(const FGameplayTag& PreviousRotationMode);

	void ApplyDesiredRotationMode(const FGameplayTag& RotationModeToApply, float DeltaSeconds);

	// Desired Stance

public:
	const FGameplayTag& GetDesiredStance() const;

private:
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredStance"))
	void SetDesiredStance(const FGameplayTag& NewDesiredStance);

protected:
	virtual void ApplyDesiredStance(const FGameplayTag& StanceToApply, float DeltaSeconds);

	// Stance

public:
	virtual bool CanCrouch() const;

public:
	const FGameplayTag& GetStance() const;

protected:
	void SetStance(const FGameplayTag& NewStance);

	void Stand(EGMC_CollisionShape CurrentCollisionShape, float DeltaSeconds);
	void Crouch(EGMC_CollisionShape CurrentCollisionShape, float DeltaSeconds);

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnStanceChanged(const FGameplayTag& PreviousStance);

	// Desired Gait

public:
	const FGameplayTag& GetDesiredGait() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredGait"))
	void SetDesiredGait(const FGameplayTag& NewDesiredGait);

private:
	void SetDesiredGait(const FGameplayTag& NewDesiredGait, bool bSendRpc);

	// Gait

public:
	const FGameplayTag& GetGait() const;

protected:
	void SetGait(const FGameplayTag& NewGait);

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnGaitChanged(const FGameplayTag& PreviousGait);

private:
	void ApplyDesiredGait(const FGameplayTag& GaitToApply, float DeltaSeconds);

	FGameplayTag CalculateMaxAllowedGait() const;

	FGameplayTag CalculateActualGait(const FGameplayTag& MaxAllowedGait) const;

	bool CanSprint() const;

	// Overlay Mode

public:
	const FGameplayTag& GetOverlayMode() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewOverlayMode"))
	void SetOverlayMode(const FGameplayTag& NewOverlayMode);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnOverlayModeChanged(const FGameplayTag& PreviousOverlayMode);

// Locomotion Action

public:
	const FGameplayTag& GetLocomotionAction() const;

	void SetLocomotionAction(const FGameplayTag& NewLocomotionAction);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction);

		// Input

protected:
	virtual void RefreshInput(float DeltaTime);

	// View

public:
	const FAlsViewState& GetViewState() const;

private:
	void RefreshView(float DeltaTime);

	// Locomotion

public:
	const FAlsLocomotionState& GetLocomotionState() const;

private:
	void RefreshLocomotionLocationAndRotation();

	void RefreshLocomotionEarly();

	void RefreshLocomotion(const float DeltaTime);

	void RefreshVelocityBlend(const float DeltaTime);

	void RefreshRotationYawOffsets(const float ViewRelativeVelocityYawAngle);

	void RefreshLocomotionLate();

	// Jumping

public:

	bool CanJump() const;
	
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnJumped();

protected:
	virtual void ApplyDesiredJump(bool bRequestedJump, float DeltaSeconds);
	
	virtual void ApplyDesiredJump_Simulated(bool bRequestedJump, float DeltaSeconds);

	
	// Rotation

private:
	void RefreshGroundedRotation(float DeltaTime);

protected:
	virtual bool RefreshCustomGroundedMovingRotation(float DeltaTime);

	virtual bool RefreshCustomGroundedNotMovingRotation(float DeltaTime);

	float CalculateGroundedMovingRotationInterpolationSpeed() const;

	void RefreshGroundedAimingRotation(float DeltaTime);

	bool ConstrainAimingRotation(FRotator& ActorRotation, float DeltaTime, bool bApplySecondaryConstraint = false);

private:
	void ApplyRotationYawSpeedAnimationCurve(float DeltaTime);

	void RefreshInAirRotation(float DeltaTime);

protected:
	virtual bool RefreshCustomInAirRotation(float DeltaTime);

	void RefreshInAirAimingRotation(float DeltaTime);

	void SetRotationSmooth(float TargetYawAngle, float DeltaTime, float InterpolationSpeed);

	void SetRotationExtraSmooth(float TargetYawAngle, float DeltaTime, float InterpolationSpeed, float TargetYawAngleRotationSpeed);

	void SetRotationInstant(float TargetYawAngle);

	void RefreshTargetYawAngleUsingLocomotionRotation();

	void SetTargetYawAngle(float TargetYawAngle);

	void SetTargetYawAngleSmooth(float TargetYawAngle, float DeltaTime, float RotationSpeed);

	void RefreshViewRelativeTargetYawAngle();

	// Rolling

// public:
// 	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
// 	void StartRolling(float PlayRate = 1.0f);
//
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	UAnimMontage* SelectRollMontage();
//
// 	bool IsRollingAllowedToStart(const UAnimMontage* Montage) const;
//
// private:
// 	void StartRolling(float PlayRate, float TargetYawAngle);
// 	
// 	void StartRollingImplementation(UAnimMontage* Montage, float PlayRate, float InitialYawAngle, float TargetYawAngle);
//
// 	void RefreshRolling(float DeltaTime);
//
// 	void RefreshRollingPhysics(float DeltaTime);
//
// 	// Mantling
//
// public:
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	bool IsMantlingAllowedToStart() const;
//
// 	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (ReturnDisplayName = "Success"))
// 	bool StartMantlingGrounded();
//
// private:
// 	bool StartMantlingInAir();
//
// 	bool StartMantling(const FAlsMantlingTraceSettings& TraceSettings);
//
// 	void StartMantlingImplementation(const FAlsMantlingParameters& Parameters);
//
// protected:
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	UAlsMantlingSettings* SelectMantlingSettings(EAlsMantlingType MantlingType);
//
// 	float CalculateMantlingStartTime(const UAlsMantlingSettings* MantlingSettings, float MantlingHeight) const;
//
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	void OnMantlingStarted(const FAlsMantlingParameters& Parameters);
//
// private:
// 	void RefreshMantling();
//
// 	void StopMantling(bool bStopMontage = false);
//
// protected:
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	void OnMantlingEnded();

	// Ragdolling

public:
	const FAlsRagdollingState& GetRagdollingState() const;
//
// 	bool IsRagdollingAllowedToStart() const;
//
// 	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
// 	void StartRagdolling();
//
// private:
// 	void StartRagdollingImplementation();
//
// // protected:
// // 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// // 	void OnRagdollingStarted();
//
// public:
// 	bool IsRagdollingAllowedToStop() const;
//
// 	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (ReturnDisplayName = "Success"))
// 	bool StopRagdolling();
//
// private:
// 	void StopRagdollingImplementation();
//
// protected:
// 	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	UAnimMontage* SelectGetUpMontage(bool bRagdollFacingUpward);
//
// 	// UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
// 	// void OnRagdollingEnded();
//
// private:
// 	void SetRagdollTargetLocation(const FVector& NewTargetLocation);
//
// 	void RefreshRagdolling(float DeltaTime);
//
// 	FVector RagdollTraceGround(bool& bGrounded) const;
//
// 	void ConstraintRagdollSpeed() const;
};


inline const FAlsMovementGaitSettings& UAlsCharacterMovementComponent::GetGaitSettings() const
{
	return GaitSettings;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetRotationMode() const
{
	return RotationMode;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetStance() const
{
	return Stance;
}

inline void UAlsCharacterMovementComponent::SetMaxAllowedGait(const FGameplayTag& NewMaxAllowedGait)
{
	MaxAllowedGait = NewMaxAllowedGait;
}

inline float UAlsCharacterMovementComponent::GetGaitAmount() const
{
	return GaitAmount;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetMaxAllowedGait() const
{
	return MaxAllowedGait;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetViewMode() const
{
	return ViewMode;
}

inline bool UAlsCharacterMovementComponent::IsDesiredAiming() const
{
	return bDesiredAiming;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetDesiredRotationMode() const
{
	return DesiredRotationMode;
}


inline const FGameplayTag& UAlsCharacterMovementComponent::GetDesiredStance() const
{
	return DesiredStance;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetDesiredGait() const
{
	return DesiredGait;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetGait() const
{
	return Gait;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetOverlayMode() const
{
	return OverlayMode;
}

inline const FGameplayTag& UAlsCharacterMovementComponent::GetLocomotionAction() const
{
	return LocomotionAction;
}

inline const FAlsViewState& UAlsCharacterMovementComponent::GetViewState() const
{
	return ViewState;
}

inline const FAlsLocomotionState& UAlsCharacterMovementComponent::GetLocomotionState() const
{
	return LocomotionState;
}

inline const FAlsRagdollingState& UAlsCharacterMovementComponent::GetRagdollingState() const
{
	return RagdollingState;
}
