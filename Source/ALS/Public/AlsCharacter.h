#pragma once

#include "AlsCharacterMovementComponent.h"
#include "GMCPawn.h"
#include "Components/CapsuleComponent.h"
#include "State/AlsLocomotionState.h"
#include "State/AlsMantlingState.h"
#include "State/AlsRagdollingState.h"
#include "State/AlsRollingState.h"
#include "State/AlsViewState.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsCharacter.generated.h"

struct FAlsMantlingParameters;
struct FAlsMantlingTraceSettings;
class UAlsCharacterMovementComponent;
class UAlsCharacterSettings;
class UAlsMovementSettings;
class UAlsAnimationInstance;
class UAlsMantlingSettings;

UCLASS(AutoExpandCategories = ("Settings|Als Character", "Settings|Als Character|Desired State"))
class ALS_API AAlsCharacter : public AGMC_Pawn
{
	GENERATED_BODY()

protected:
	//Taken from Character.h
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, Category = "ACharacter",
		meta=(AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	//Taken from Character.h
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, Category = "ACharacter",
		meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;
	
	UPROPERTY(BlueprintReadOnly, Category = "Als Character")
	TObjectPtr<UAlsCharacterMovementComponent> AlsCharacterMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Meta = (ShowInnerProperties))
	TWeakObjectPtr<UAlsAnimationInstance> AnimationInstance;

public:
	explicit AAlsCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static FName MeshComponentName;
	static FName CharacterMovementComponentName;
	static FName CapsuleComponentName;
	static FName FPSComponentName;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	virtual void PostRegisterAllComponents() override;

	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay() override;

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;

public:

	virtual void Tick(float DeltaTime) override;

	virtual void PossessedBy(AController* NewController) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character", Meta = (ReturnDisplayName = "Handled"))
	bool OnCalculateCamera(float DeltaTime, FMinimalViewInfo& ViewInfo);

private:
	void RefreshMeshProperties() const;

public:
	TObjectPtr<USkeletalMeshComponent> GetMesh() const;

	TWeakObjectPtr<UAlsAnimationInstance>  GetAnimInstance() const;

	TObjectPtr<UCapsuleComponent> GetCapsuleComponent() const;
	
	TObjectPtr<UAlsCharacterMovementComponent> GetCharacterMovement() const;
public:
	const FGameplayTag& GetViewMode() const;
	
	const FGameplayTag& GetLocomotionMode() const;
	
	bool IsDesiredAiming() const;
	
	const FGameplayTag& GetDesiredRotationMode() const;
	
	const FGameplayTag& GetRotationMode() const;
	
	const FGameplayTag& GetDesiredStance() const;
	
	const FGameplayTag& GetStance() const;
	
	const FGameplayTag& GetDesiredGait() const;
	
	const FGameplayTag& GetGait() const;
	
	const FGameplayTag& GetOverlayMode() const;
	
	const FGameplayTag& GetLocomotionAction() const;
	
	const FVector& GetInputDirection() const;

	virtual FRotator GetViewRotation() const override;
	
	const FAlsViewState& GetViewState() const;
	
	const FAlsLocomotionState& GetLocomotionState() const;

	const FAlsRagdollingState& GetRagdollingState() const;


private:
	virtual void FaceRotation(FRotator Rotation, float DeltaTime) override final;

	// Debug

public:
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& Unused, float& VerticalLocation) override;

private:
	static void DisplayDebugHeader(const UCanvas* Canvas, const FText& HeaderText, const FLinearColor& HeaderColor,
	                               float Scale, float HorizontalLocation, float& VerticalLocation);

	void DisplayDebugCurves(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugState(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugShapes(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugTraces(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	void DisplayDebugMantling(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;
};

inline TObjectPtr<USkeletalMeshComponent>  AAlsCharacter::GetMesh() const
{
	return Mesh;
}

inline TWeakObjectPtr<UAlsAnimationInstance> AAlsCharacter::GetAnimInstance() const
{
	return AnimationInstance;
}

inline TObjectPtr<UCapsuleComponent>  AAlsCharacter::GetCapsuleComponent() const
{
	return Cast<UCapsuleComponent>(RootComponent);
}

inline TObjectPtr<UAlsCharacterMovementComponent> AAlsCharacter::GetCharacterMovement() const
{
	return AlsCharacterMovement;
}

inline const FGameplayTag& AAlsCharacter::GetViewMode() const
{
	return AlsCharacterMovement->ViewMode;
}

inline bool AAlsCharacter::IsDesiredAiming() const
{
	return AlsCharacterMovement->bDesiredAiming;
}

inline const FGameplayTag& AAlsCharacter::GetDesiredRotationMode() const
{
	return AlsCharacterMovement->DesiredRotationMode;
}

inline const FGameplayTag& AAlsCharacter::GetRotationMode() const
{
	return AlsCharacterMovement->RotationMode;
}

inline const FGameplayTag& AAlsCharacter::GetDesiredStance() const
{
	return AlsCharacterMovement->DesiredStance;
}

inline const FGameplayTag& AAlsCharacter::GetStance() const
{
	return AlsCharacterMovement->Stance;
}

inline const FGameplayTag& AAlsCharacter::GetDesiredGait() const
{
	return AlsCharacterMovement->DesiredGait;
}

inline const FGameplayTag& AAlsCharacter::GetGait() const
{
	return AlsCharacterMovement->Gait;
}

inline const FGameplayTag& AAlsCharacter::GetOverlayMode() const
{
	return AlsCharacterMovement->OverlayMode;
}

inline const FGameplayTag& AAlsCharacter::GetLocomotionAction() const
{
	return AlsCharacterMovement->LocomotionAction;
}

inline const FVector& AAlsCharacter::GetInputDirection() const
{
	return AlsCharacterMovement->InputDirection;
}

inline const FAlsViewState& AAlsCharacter::GetViewState() const
{
	return AlsCharacterMovement->ViewState;
}

inline const FAlsLocomotionState& AAlsCharacter::GetLocomotionState() const
{
	return AlsCharacterMovement->LocomotionState;
}

inline const FAlsRagdollingState& AAlsCharacter::GetRagdollingState() const
{
	return AlsCharacterMovement->RagdollingState;
}
