// Fill out your copyright notice in the Description page of Project Settings.

#include "AlsCharacterMovementComponent.h"

#include "AlsAnimationInstance.h"
#include "EnhancedInputComponent.h"
#include "AlsCharacter.h"
#include "Camera/AlsCameraComponent.h"
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
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopWalkAction);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartCrouchAction);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopCrouchAction);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &UAlsCharacterMovementComponent::StartJumpAction);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopJumpAction);
		
		EnhancedInputComponent->BindAction(RotationModeAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartRotationModeAction);
		EnhancedInputComponent->BindAction(RotationModeAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopRotationModeAction);
		EnhancedInputComponent->BindAction(ViewModeAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartViewModeAction);
		EnhancedInputComponent->BindAction(ViewModeAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopViewModeAction);
		EnhancedInputComponent->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartSwitchShoulderAction);
		EnhancedInputComponent->BindAction(SwitchShoulderAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopSwitchShoulderAction);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartAimAction);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopAimAction);
		EnhancedInputComponent->BindAction(RagdollAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartRagdollAction);
		EnhancedInputComponent->BindAction(RagdollAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopRagdollAction);

		EnhancedInputComponent->BindAction(LeanLeftAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartLeanLeftAction);
		EnhancedInputComponent->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopLeanLeftAction);
		EnhancedInputComponent->BindAction(LeanRightAction, ETriggerEvent::Triggered, this, &UAlsCharacterMovementComponent::StartLeanRightAction);
		EnhancedInputComponent->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &UAlsCharacterMovementComponent::StopLeanRightAction);
	}
}

void UAlsCharacterMovementComponent::ApplyViewInput(const FInputActionInstance& InputAction)
{
	Super::ApplyViewInput(InputAction);

	// const FVector& Value = InputAction.GetValue().Get<FVector>();
	// float newX;
	// bool addYaw;
	// float newY;
	// bool addPitch;
	//
	// //add in sensitivty scale here set via player settings
	// if (USKGShooterPawnComponent* FPSComponent = CharacterOwner->GetFPSComponent())
	// {
	// 	FPSComponent->GetSensitivityMultiplier(Value.X, 1.0f, Value.Y, 1.0f, newX, addYaw, newY, addPitch);
	// 	if (addYaw)
	// 	{
	// 		GetGMCPawnOwner()->AddControllerYawInput(newX);
	// 	}
	// 	if (addPitch)
	// 	{
	// 		GetGMCPawnOwner()->AddControllerPitchInput(-newY);
	// 	}
	// 	FPSComponent->SetMouseInput(newX, -newY);
	// }
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
	SetDesiredGait(AlsGaitTags::Walking);
}

void UAlsCharacterMovementComponent::StopWalkAction(const FInputActionInstance& InputAction)
{
	SetDesiredGait(AlsGaitTags::Running);
}

void UAlsCharacterMovementComponent::StartCrouchAction(const FInputActionInstance& InputAction)
{
	SetDesiredStance(AlsStanceTags::Crouching);
}

void UAlsCharacterMovementComponent::StopCrouchAction(const FInputActionInstance& InputAction)
{
	return;
}
	
void UAlsCharacterMovementComponent::StartJumpAction(const FInputActionInstance& InputAction)
{
	if (GetStance() != AlsStanceTags::Standing)
	{
		SetDesiredStance(AlsStanceTags::Standing);
		return;
	}
	bWantsToJump = true;
}
	
void UAlsCharacterMovementComponent::StopJumpAction(const FInputActionInstance& InputAction)
{
	bWantsToJump = false;
}

void UAlsCharacterMovementComponent::StartRotationModeAction(const FInputActionInstance& InputAction)
{
	SetDesiredRotationMode(GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
							   ? AlsRotationModeTags::ViewDirection
							   : AlsRotationModeTags::VelocityDirection);
}
	
void UAlsCharacterMovementComponent::StopRotationModeAction(const FInputActionInstance& InputAction)
{
	return;
}

void UAlsCharacterMovementComponent::StartViewModeAction(const FInputActionInstance& InputAction)
{
	SetViewMode(GetViewMode() == AlsViewModeTags::ThirdPerson ? AlsViewModeTags::FirstPerson : AlsViewModeTags::ThirdPerson);
}
	
void UAlsCharacterMovementComponent::StopViewModeAction(const FInputActionInstance& InputAction)
{
	return;
}

void UAlsCharacterMovementComponent::StartSwitchShoulderAction(const FInputActionInstance& InputAction)
{
	CharacterOwner->GetCamera()->SetRightShoulder(!CharacterOwner->GetCamera()->IsRightShoulder());
}
	
void UAlsCharacterMovementComponent::StopSwitchShoulderAction(const FInputActionInstance& InputAction)
{
	return;
}

void UAlsCharacterMovementComponent::StartAimAction(const FInputActionInstance& InputAction)
{
	// CharacterOwner->GetFPSComponent()->StartAiming();
	// bDesiredAiming = true;
}
	
void UAlsCharacterMovementComponent::StopAimAction(const FInputActionInstance& InputAction)
{
	// CharacterOwner->GetFPSComponent()->StopAiming();
	// bDesiredAiming = false;
}

void UAlsCharacterMovementComponent::StartRagdollAction(const FInputActionInstance& InputAction)
{
	bWantsToRagdoll = !bWantsToRagdoll;
}
	
void UAlsCharacterMovementComponent::StopRagdollAction(const FInputActionInstance& InputAction)
{
	return;
}

void UAlsCharacterMovementComponent::StartLeanLeftAction(const FInputActionInstance& InputAction)
{
	SetDesiredLeanLeft(true);
}
void UAlsCharacterMovementComponent::StopLeanLeftAction(const FInputActionInstance& InputAction)
{
	SetDesiredLeanLeft(false);
}

void UAlsCharacterMovementComponent::StartLeanRightAction(const FInputActionInstance& InputAction)
{
	SetDesiredLeanRight(true);
}
void UAlsCharacterMovementComponent::StopLeanRightAction(const FInputActionInstance& InputAction)
{
	SetDesiredLeanRight(false);
}