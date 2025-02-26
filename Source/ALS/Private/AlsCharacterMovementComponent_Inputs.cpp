//  Fill out your copyright notice in the Description page of Project Settings.

#include "AlsCharacterMovementComponent.h"

#include "AlsAnimationInstance.h"
#include "EnhancedInputComponent.h"
#include "AlsCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

void UAlsCharacterMovementComponent::SetupPlayerInputComponent_Implementation(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent_Implementation(PlayerInputComponent);

	gmc_ck(PlayerInputComponent)

	if (const auto& EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartSprintAction);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopSprintAction);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartWalkAction);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartCrouchAction);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &UAlsCharacterMovementComponent::StartJumpAction);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopJumpAction);
		
		EnhancedInputComponent->BindAction(RotationModeAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartRotationModeAction);
		EnhancedInputComponent->BindAction(ViewModeAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartViewModeAction);
		EnhancedInputComponent->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartSwitchShoulderAction);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartAimAction);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopAimAction);
		EnhancedInputComponent->BindAction(RagdollAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartRagdollAction);
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartRollAction);
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopRollAction);
	}
}

void UAlsCharacterMovementComponent::StartSprintAction(const FInputActionInstance& InputAction)
{
	SetDesiredGait(AlsGaitTags::Sprinting);
}

void UAlsCharacterMovementComponent::StopSprintAction(const FInputActionInstance& InputAction)
{
	SetDesiredGait(AlsGaitTags::Running);
}

void UAlsCharacterMovementComponent::StartWalkAction(const FInputActionInstance& InputAction)
{
	SetDesiredGait(GetDesiredGait() == AlsGaitTags::Walking
							   ? AlsGaitTags::Running
							   : AlsGaitTags::Walking);
}

void UAlsCharacterMovementComponent::StartCrouchAction(const FInputActionInstance& InputAction)
{
	SetDesiredStance(GetDesiredStance() == AlsStanceTags::Crouching
							   ? AlsStanceTags::Standing
							   : AlsStanceTags::Crouching);
}
	
void UAlsCharacterMovementComponent::StartJumpAction(const FInputActionInstance& InputAction)
{
	if (GetStance() != AlsStanceTags::Standing)
	{
		SetDesiredStance(AlsStanceTags::Standing);
		return;
	}
	bDesiredJumping = true;
}
	
void UAlsCharacterMovementComponent::StopJumpAction(const FInputActionInstance& InputAction)
{
	bDesiredJumping = false;
}

void UAlsCharacterMovementComponent::StartRotationModeAction(const FInputActionInstance& InputAction)
{
	SetDesiredRotationMode(GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
							   ? AlsRotationModeTags::ViewDirection
							   : AlsRotationModeTags::VelocityDirection);
}

void UAlsCharacterMovementComponent::StartViewModeAction(const FInputActionInstance& InputAction)
{
	SetViewMode(GetViewMode() == AlsViewModeTags::ThirdPerson ? AlsViewModeTags::FirstPerson : AlsViewModeTags::ThirdPerson);
}

void UAlsCharacterMovementComponent::StartSwitchShoulderAction(const FInputActionInstance& InputAction)
{
	CharacterOwner->GetCamera()->SetRightShoulder(!CharacterOwner->GetCamera()->IsRightShoulder());
}

void UAlsCharacterMovementComponent::StartAimAction(const FInputActionInstance& InputAction)
{
	bDesiredAiming = true;
}
	
void UAlsCharacterMovementComponent::StopAimAction(const FInputActionInstance& InputAction)
{
	bDesiredAiming = false;
}

void UAlsCharacterMovementComponent::StartRagdollAction(const FInputActionInstance& InputAction)
{
	bDesiredRagdolling = !bDesiredRagdolling;
}

void UAlsCharacterMovementComponent::StartRollAction(const FInputActionInstance& InputAction)
{
	bDesiredRolling = true;
}
	
void UAlsCharacterMovementComponent::StopRollAction(const FInputActionInstance& InputAction)
{
	bDesiredRolling = false;
}