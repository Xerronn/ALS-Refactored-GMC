#pragma once

#include "AlsCharacterMovementComponent.h"
#include "GMCPawn.h"
#include "Camera/AlsCameraComponent.h"
#include "Components/CapsuleComponent.h"
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

	//Taken from Character.h
	/** Saved rotation offset of mesh. */
	UPROPERTY()
	FQuat BaseRotationOffset;

	/** Saved translation offset of mesh. */
	UPROPERTY()
	FVector BaseTranslationOffset;

public:
	explicit AAlsCharacter();

	static FName MeshComponentName;
	static FName CharacterMovementComponentName;
	static FName CapsuleComponentName;
	static FName FPSComponentName;

	virtual void PostInitializeComponents() override;

	/** Get the saved translation offset of mesh. This is how much extra offset is applied from the center of the capsule. */
	UFUNCTION(BlueprintCallable, Category=Character)
	FVector GetBaseTranslationOffset() const { return BaseTranslationOffset; }

	/** Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	virtual FQuat GetBaseRotationOffset() const { return BaseRotationOffset; }

protected:
	virtual void BeginPlay() override;

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character", Meta = (ReturnDisplayName = "Handled"))
	bool OnCalculateCamera(float DeltaTime, FMinimalViewInfo& ViewInfo);

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Als Character")
	void AttachOverlayObject(UStaticMesh* NewStaticMesh, USkeletalMesh* NewSkeletalMesh,
		TSubclassOf<UAnimInstance> NewAnimClass, FName Socket, bool bUseLeftGunBone);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Als Character")
	void ClearOverlayObject();
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Als Character")
	void RefreshOverlayObject();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Als Character")
	void RefreshOverlayLinkedAnimationLayer();

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
