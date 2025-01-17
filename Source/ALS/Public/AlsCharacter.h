#pragma once

#include "AlsCharacterMovementComponent.h"
#include "GMCPawn.h"
#include "Camera/AlsCameraComponent.h"
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
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, Category = "Character",
		meta=(AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	//Taken from Character.h
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, Category = "Character",
		meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Als Character")
	TObjectPtr<UAlsCharacterMovementComponent> AlsCharacterMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Meta = (ShowInnerProperties))
	TWeakObjectPtr<UAlsAnimationInstance> AnimationInstance;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TObjectPtr<UAlsCameraComponent> Camera;

public:
	explicit AAlsCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static FName MeshComponentName;
	static FName CharacterMovementComponentName;
	static FName CapsuleComponentName;
	static FName FPSComponentName;

	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay() override;

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;

public:

	virtual void Tick(float DeltaTime) override;

	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnJump();

	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnOverlayModeChanged(const FGameplayTag& PreviousOverlayMode);

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

	TObjectPtr<UAlsCameraComponent> GetCamera() const;
	
	const FGameplayTag GetLocomotionMode() const;

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

inline TObjectPtr<UAlsCameraComponent> AAlsCharacter::GetCamera() const
{
	return Camera;
}
