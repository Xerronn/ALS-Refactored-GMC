#include "Camera/AlsCameraAnimationInstance.h"

#include "Camera/AlsCameraComponent.h"
#include "AlsCharacter.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCameraAnimationInstance)

void UAlsCameraAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<AAlsCharacter>(GetOwningActor());
	Camera = Cast<UAlsCameraComponent>(GetSkelMeshComponent());

#if WITH_EDITOR
	const auto* World{GetWorld()};

	if (IsValid(World) && !World->IsGameWorld())
	{
		// Use default objects for editor preview.

		if (!IsValid(Character))
		{
			Character = GetMutableDefault<AAlsCharacter>();
		}

		if (!IsValid(Camera))
		{
			Camera = GetMutableDefault<UAlsCameraComponent>();
		}
	}
#endif
}

void UAlsCameraAnimationInstance::NativeUpdateAnimation(const float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (!IsValid(Character) || !IsValid(Camera) || !IsValid(Character->GetCharacterMovement()))
	{
		return;
	}

	ViewMode = Character->GetCharacterMovement()->GetViewMode();
	LocomotionMode = Character->GetLocomotionMode();
	
	if (ViewMode != AlsViewModeTags::FirstPerson)
	{
		RotationMode = Character->GetCharacterMovement()->GetRotationMode();
	}
	else
	{
		// In first-person mode, the rotation mode is always view direction, which can lead to weird camera movement when switching between
		// the first-person view mode and the velocity direction rotation mode, so to fix this we use the desired rotation mode here.
		RotationMode = Character->GetCharacterMovement()->GetDesiredRotationMode();
	}
	
	Stance = Character->GetCharacterMovement()->GetStance();
	Gait = Character->GetCharacterMovement()->GetGait();
	LocomotionAction = Character->GetCharacterMovement()->GetLocomotionAction();

	bRightShoulder = Camera->IsRightShoulder();
}
