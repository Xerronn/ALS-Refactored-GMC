#include "AlsCharacterMovementComponent.h"

#include "AlsCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Settings/AlsCharacterSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsRotation.h"
#include "Utility/AlsUtility.h"
#include "Utility/AlsVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacterMovementComponent)

UAlsCharacterMovementComponent::UAlsCharacterMovementComponent()
{
	GroundFriction = 0.3f;
	InputAccelerationGrounded = 2048.f;
	FallControl = 0.15f;

	RotationRate = 0.f;
	bOrientToControlRotationDirection = false;
	bOrientToInputDirection = false;

	WalkableFloorAngle = 45.f;
	MaxStepUpHeight = 50.f;
	MaxStepDownHeight = 50.f;

	bCanWalkOffLedges = true;
	LedgeFallOffThreshold = 0.5f;

	NavAgentProps.bCanCrouch = true;

	bNoBlueprintEvents = true;
}

#if WITH_EDITOR
bool UAlsCharacterMovementComponent::CanEditChange(const FProperty* Property) const
{
	return Super::CanEditChange(Property) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, RotationRate) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bOrientToInputDirection) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bOrientToControlRotationDirection);
}
#endif

namespace AlsCharacterConstants
{
	constexpr auto TeleportDistanceThresholdSquared{FMath::Square(50.0f)};
	constexpr auto MinAimingYawAngleLimit{70.0f};
}

void UAlsCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<AAlsCharacter>(PawnOwner);
	
	if (!IsValid(CharacterOwner) || !GetGMCPawnOwner())
	{
		return;
	}

	ALS_ENSURE(Settings);
	ALS_ENSURE(MovementSettings);
	RefreshGaitSettings();

	RotationMode = bDesiredAiming ? AlsRotationModeTags::Aiming : DesiredRotationMode;
	Stance = DesiredStance;
	Gait = DesiredGait;
	
	ViewState.Rotation = GetControllerRotation_GMC();
	ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw);

	const auto& ActorTransform{GetActorTransform_GMC()};

	LocomotionState.Location = ActorTransform.GetLocation();
	LocomotionState.Rotation = GetActorRotation_GMC();
	LocomotionState.PreviousYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);

	RefreshTargetYawAngleUsingLocomotionRotation();

	LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
	LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);

	const FVector Extent = GetRootCollisionExtent(true);
	StandingHalfHeight = Extent.Z;
	DefaultRadius = Extent.X;
}

void UAlsCharacterMovementComponent::BindReplicationData_Implementation()
{
	Super::BindReplicationData_Implementation();

	//input
	BindBool(
		bDesiredJumping,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		EGMC_InterpolationFunction::NearestNeighbour
	);

	BindByte(
		bDesiredAiming,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		EGMC_InterpolationFunction::NearestNeighbour
	);
	
	// BindBool(
	// 	bWantsToRagdoll,
	// 	EGMC_PredictionMode::ClientAuth_Input,
	// 	EGMC_CombineMode::CombineIfUnchanged,
	// 	EGMC_SimulationMode::PeriodicAndOnChange_Output,
	// 	EGMC_InterpolationFunction::NearestNeighbour
	// );

	BindGameplayTag(
		DesiredStance,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);

	BindGameplayTag(
		DesiredGait,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);

	BindGameplayTag(
		DesiredRotationMode,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);

	BindGameplayTag(
		ViewMode,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);

	BindGameplayTag(
		OverlayMode,
		EGMC_PredictionMode::ClientAuth_Input,
		EGMC_CombineMode::CombineIfUnchanged,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);
	
	//end of input

	BindBool(
		bJustJumped,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::PeriodicAndOnChange_Output,
		EGMC_InterpolationFunction::NearestNeighbour
	);

	//limiters
	BindBool(
		bCanJump,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindGameplayTag(
		MaxAllowedGait,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);
	
	BindCompressedSinglePrecisionFloat(
		MaxDesiredSpeed,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);
	//end of limiters

	//current state
	BindGameplayTag(
		Stance,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindGameplayTag(
		Gait,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindGameplayTag(
		RotationMode,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindGameplayTag(
		LocomotionAction,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::Periodic_Output,
		{}
	);
	
	BindCompressedRotator(
		ViewState.Rotation,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindCompressedRotator(
		LocomotionState.Rotation,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);

	BindCompressedVector(
		LocomotionState.Velocity,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);
	
	BindCompressedSinglePrecisionFloat(
		LocomotionState.SmoothTargetYawAngle,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::None,
		{}
	);
	//end of state

	BindCompressedVector(
		RagdollTargetLocation,
		EGMC_PredictionMode::ServerAuth_Output_ClientValidated,
		EGMC_CombineMode::AlwaysCombine,
		EGMC_SimulationMode::Periodic_Output,
		EGMC_InterpolationFunction::NearestNeighbour
	);
}

void UAlsCharacterMovementComponent::ClampToValidValues()
{
	Super::ClampToValidValues();
	
	CrouchedHalfHeight = FMath::Clamp(CrouchedHalfHeight, KINDA_SMALL_NUMBER, StandingHalfHeight);
	ProneHalfHeight = FMath::Clamp(ProneHalfHeight, KINDA_SMALL_NUMBER, StandingHalfHeight);
}

void UAlsCharacterMovementComponent::PreMovementUpdate_Implementation(float DeltaSeconds)
{
	Super::PreMovementUpdate_Implementation(DeltaSeconds);

	if (GetIterationNumber() == 1)
	{
		bJustJumped = false;
		// bJustLanded = false;
	}
}

void UAlsCharacterMovementComponent::MovementUpdate_Implementation(float DeltaSeconds)
{
	Super::MovementUpdate_Implementation(DeltaSeconds);

	RefreshInput(DeltaSeconds);

	RefreshLocomotionEarly();

	RefreshView(DeltaSeconds);
	RefreshLocomotion();
	ApplyDesiredStance(DesiredStance, DeltaSeconds);
	MaintainMeshOffset();
	ApplyDesiredGait(DesiredGait, DeltaSeconds);
	ApplyDesiredRotationMode(DesiredRotationMode, DeltaSeconds);

	RefreshGroundedRotation(DeltaSeconds);
	RefreshInAirRotation(DeltaSeconds);

	// StartMantlingInAir();
	// RefreshMantling();
	// RefreshRagdolling(DeltaSeconds);
	// RefreshRolling(DeltaTime);

	ApplyDesiredJump(bDesiredJumping, DeltaSeconds);

	// ApplyDesiredRagdoll(bWantsToRagdoll, DeltaSeconds);

	RefreshLocomotionLate();
}

void UAlsCharacterMovementComponent::MovementUpdateSimulated_Implementation(float DeltaSeconds)
{
	Super::MovementUpdate_Implementation(DeltaSeconds);
	
	RefreshInput(DeltaSeconds);

	RefreshLocomotionEarly();

	RefreshView(DeltaSeconds);
	RefreshLocomotion();
	ApplyDesiredStance(DesiredStance, DeltaSeconds);
	MaintainMeshOffsetSimulated();
	ApplyDesiredGait(DesiredGait, DeltaSeconds);
	ApplyDesiredRotationMode(DesiredRotationMode, DeltaSeconds);

	RefreshGroundedRotation(DeltaSeconds);
	RefreshInAirRotation(DeltaSeconds);

	// StartMantlingInAir();
	// RefreshMantling();
	// RefreshRagdolling(DeltaSeconds);
	// RefreshRolling(DeltaTime);

	ApplyDesiredJump_Simulated(bJustJumped, DeltaSeconds);

	// ApplyDesiredRagdoll(bWantsToRagdoll, DeltaSeconds);

	RefreshLocomotionLate();
}

void UAlsCharacterMovementComponent::SetMovementSettings(UAlsMovementSettings* NewMovementSettings)
{
	ALS_ENSURE(IsValid(NewMovementSettings));

	MovementSettings = NewMovementSettings;

	RefreshGaitSettings();
}

void UAlsCharacterMovementComponent::RefreshGaitSettings()
{
	if (!ALS_ENSURE(IsValid(MovementSettings)))
	{
		return;
	}

	const auto* StanceSettings{MovementSettings->RotationModes.Find(RotationMode)};
	const auto* NewGaitSettings{ALS_ENSURE(StanceSettings != nullptr) ? StanceSettings->Stances.Find(Stance) : nullptr};

	GaitSettings = ALS_ENSURE(NewGaitSettings != nullptr) ? *NewGaitSettings : FAlsMovementGaitSettings{};
}

void UAlsCharacterMovementComponent::RefreshGroundedMovementSettings()
{
	auto WalkSpeed{GaitSettings.WalkForwardSpeed};
	auto RunSpeed{GaitSettings.RunForwardSpeed};

	if (GaitSettings.bAllowDirectionDependentMovementSpeed &&
	    Velocity.SizeSquared() > UE_KINDA_SMALL_NUMBER &&
	    IsValid(MovementSettings))
	{
		const auto* Controller{GetController()};

		const auto ViewRotation{
			IsValid(Controller)
				? GetController()->GetControlRotation()
				: CharacterOwner->GetViewRotation()
		};

		// Ideally we should use actor rotation here instead of view rotation, but we can't do that because ALS has
		// full control over actor rotation and it is not synchronized over the network, so it would cause jitter.
		
		const auto RelativeViewRotation{UAlsRotation::GetTwist(ViewRotation.Quaternion(), FVector::DownVector)};

		const FVector2D RelativeVelocity{RelativeViewRotation.UnrotateVector(Velocity)};
		const auto VelocityAngle{UAlsVector::DirectionToAngle(RelativeVelocity)};

		const auto ForwardSpeedAmount{
			FMath::GetMappedRangeValueClamped(MovementSettings->VelocityAngleToSpeedInterpolationRange,
			                                  {1.0f, 0.0f}, FMath::Abs(VelocityAngle))
		};

		WalkSpeed = FMath::Lerp(GaitSettings.WalkBackwardSpeed, GaitSettings.WalkForwardSpeed, ForwardSpeedAmount);
		RunSpeed = FMath::Lerp(GaitSettings.RunBackwardSpeed, GaitSettings.RunForwardSpeed, ForwardSpeedAmount);
	}

	// Map the character's current speed to the to the speed ranges from the movement settings. This allows
	// us to vary movement speeds but still use the mapped range in calculations for consistent results.

	const auto Speed{UE_REAL_TO_FLOAT(Velocity.Size2D())};

	if (Speed > RunSpeed)
	{
		GaitAmount = FMath::GetMappedRangeValueClamped(FVector2f{RunSpeed, GaitSettings.SprintSpeed}, {2.0f, 3.0f}, Speed);
	}
	else if (Speed > WalkSpeed)
	{
		GaitAmount = FMath::GetMappedRangeValueClamped(FVector2f{WalkSpeed, RunSpeed}, {1.0f, 2.0f}, Speed);
	}
	else
	{
		GaitAmount = FMath::GetMappedRangeValueClamped(FVector2f{0.0f, WalkSpeed}, {0.0f, 1.0f}, Speed);
	}

	if (MaxAllowedGait == AlsGaitTags::Walking)
	{
		MaxDesiredSpeed = WalkSpeed;
	}
	else if (MaxAllowedGait == AlsGaitTags::Running)
	{
		MaxDesiredSpeed = RunSpeed;
	}
	else if (MaxAllowedGait == AlsGaitTags::Sprinting)
	{
		MaxDesiredSpeed = GaitSettings.SprintSpeed;
	}
	else
	{
		MaxDesiredSpeed = GaitSettings.RunForwardSpeed;
	}

	// Get acceleration, deceleration and ground friction using a curve. This
	// allows us to precisely control the movement behavior at each speed.

	if (ALS_ENSURE(IsValid(GaitSettings.AccelerationAndDecelerationAndGroundFrictionCurve)))
	{
		const auto& AccelerationAndDecelerationAndGroundFrictionCurves{
			GaitSettings.AccelerationAndDecelerationAndGroundFrictionCurve->FloatCurves
		};

		InputAccelerationGrounded = AccelerationAndDecelerationAndGroundFrictionCurves[0].Eval(GaitAmount);
		BrakingDecelerationGrounded = AccelerationAndDecelerationAndGroundFrictionCurves[1].Eval(GaitAmount);
		GroundFriction = AccelerationAndDecelerationAndGroundFrictionCurves[2].Eval(GaitAmount);
	}
}

void UAlsCharacterMovementComponent::SetMovementModeLocked(const bool bNewMovementModeLocked)
{
	bMovementModeLocked = bNewMovementModeLocked;
}

void UAlsCharacterMovementComponent::SetInputBlocked(const bool bNewInputBlocked)
{
	bInputBlocked = bNewInputBlocked;
}

bool UAlsCharacterMovementComponent::TryConsumePrePenetrationAdjustmentVelocity(FVector& OutVelocity)
{
	if (!bPrePenetrationAdjustmentVelocityValid)
	{
		OutVelocity = FVector::ZeroVector;
		return false;
	}

	OutVelocity = PrePenetrationAdjustmentVelocity;

	PrePenetrationAdjustmentVelocity = FVector::ZeroVector;
	bPrePenetrationAdjustmentVelocityValid = false;

	return true;
}

void UAlsCharacterMovementComponent::SetViewMode(const FGameplayTag& NewViewMode)
{
	ViewMode = NewViewMode;
}

void UAlsCharacterMovementComponent::OnMovementModeChanged_Implementation(EGMC_MovementMode PreviousMovementMode)
{
	//just landed
	if (IsMovingOnGround() &&
		PreviousMovementMode == EGMC_MovementMode::Airborne)
	{
		if (Settings->Ragdolling.bStartRagdollingOnLand &&
		    LocomotionState.Velocity.Z <= -Settings->Ragdolling.RagdollingOnLandSpeedThreshold)
		{
			// StartRagdolling();
		}
		else if (Settings->Rolling.bStartRollingOnLand &&
		         LocomotionState.Velocity.Z <= -Settings->Rolling.RollingOnLandSpeedThreshold)
		{
			static constexpr auto PlayRate{1.3f};

			// StartRolling(PlayRate, LocomotionState.bHasVelocity
			// 	                       ? LocomotionState.VelocityYawAngle
			// 	                       : UE_REAL_TO_FLOAT(FMath::UnwindDegrees(GetActorRotation_GMC().Yaw)));
		}
		else
		{
			//todo:
			// // Increase friction for a short period of time to prevent sliding on the ground after landing. Can be done using a bound timer, and increasing friction while its >0
			//
			// static constexpr auto HasInputBrakingFrictionFactor{0.5f};
			// static constexpr auto NoInputBrakingFrictionFactor{3.0f};
			//
			// GetCharacterMovement()->BrakingFrictionFactor = LocomotionState.bHasInput
			// 	                                                ? HasInputBrakingFrictionFactor
			// 	                                                : NoInputBrakingFrictionFactor;
			//
			// static constexpr auto ResetDelay{0.5f};
			//
			// GetWorldTimerManager().SetTimer(BrakingFrictionFactorResetTimer,
			//                                 FTimerDelegate::CreateWeakLambda(this, [this]
			//                                 {
			// 	                                GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
			//                                 }), ResetDelay, false);

			// Block rotation towards the last input direction after landing to prevent
			// legs from twisting into a spiral while the landing animation is playing.

			LocomotionState.bRotationTowardsLastInputDirectionBlocked = true;
		}
	}
	//rolled off a cliff
	else if (IsAirborne() &&
	         LocomotionAction == AlsLocomotionActionTags::Rolling &&
	         Settings->Rolling.bInterruptRollingWhenInAir)
	{
		// If the character is currently rolling, then enable ragdolling.

		// StartRagdolling();
	}
	
	Super::OnMovementModeChanged_Implementation(PreviousMovementMode);
}

void UAlsCharacterMovementComponent::SetDesiredAiming(const bool bNewDesiredAiming)
{
	if (bDesiredAiming == bNewDesiredAiming)
	{
		return;
	}

	bDesiredAiming = bNewDesiredAiming;

	OnDesiredAimingChanged(!bDesiredAiming);
}

void UAlsCharacterMovementComponent::OnDesiredAimingChanged_Implementation(const bool bPreviousDesiredAiming) {}

void UAlsCharacterMovementComponent::SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode)
{
	DesiredRotationMode = NewDesiredRotationMode;
}

void UAlsCharacterMovementComponent::SetRotationMode(const FGameplayTag& NewRotationMode)
{
	if (RotationMode != NewRotationMode)
	{
		const auto PreviousRotationMode{RotationMode};

		RotationMode = NewRotationMode;

		OnRotationModeChanged(PreviousRotationMode);
	}
}

void UAlsCharacterMovementComponent::OnRotationModeChanged_Implementation(const FGameplayTag& PreviousRotationMode) {}

void UAlsCharacterMovementComponent::ApplyDesiredRotationMode(const FGameplayTag& RotationModeToApply, float DeltaSeconds)
{
	const auto bAiming{bDesiredAiming || RotationModeToApply == AlsRotationModeTags::Aiming};
	const auto bSprinting{GetMaxAllowedGait() == AlsGaitTags::Sprinting};

	if (ViewMode == AlsViewModeTags::FirstPerson)
	{
		if (IsAirborne())
		{
			if (bAiming && Settings->bAllowAimingWhenInAir)
			{
				SetRotationMode(AlsRotationModeTags::Aiming);
			}
			else
			{
				SetRotationMode(AlsRotationModeTags::ViewDirection);
			}

			return;
		}

		// Grounded and other locomotion modes.

		if (bAiming && (!bSprinting || !Settings->bSprintHasPriorityOverAiming))
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}

		return;
	}

	// Third person and other view modes.

	if (IsAirborne())
	{
		if (bAiming && Settings->bAllowAimingWhenInAir)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}
		else
		{
			SetRotationMode(RotationModeToApply);
		}

		return;
	}

	// Grounded and other locomotion modes.

	if (bSprinting)
	{
		if (bAiming && !Settings->bSprintHasPriorityOverAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (Settings->bRotateToVelocityWhenSprinting)
		{
			SetRotationMode(AlsRotationModeTags::VelocityDirection);
		}
		else
		{
			SetRotationMode(RotationModeToApply);
		}
	}
	else // Not sprinting.
	{
		if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(RotationModeToApply);
		}
	}
}

void UAlsCharacterMovementComponent::SetDesiredStance(const FGameplayTag& NewDesiredStance)
{
	if (DesiredStance == NewDesiredStance)
	{
		return;
	}

	DesiredStance = NewDesiredStance;
}

void UAlsCharacterMovementComponent::ApplyDesiredStance(const FGameplayTag& StanceToApply, float DeltaSeconds)
{
	EGMC_CollisionShape CurrentCollisionShape = GetRootCollisionShape();

	if (!LocomotionAction.IsValid())
	{
		if (IsMovingOnGround())
		{
			if (DesiredStance == AlsStanceTags::Standing)
			{
				Stand(CurrentCollisionShape, DeltaSeconds);
				SetStance(AlsStanceTags::Standing);
			}
			else if (DesiredStance == AlsStanceTags::Crouching)
			{
				Crouch(CurrentCollisionShape, DeltaSeconds);
				SetStance(AlsStanceTags::Crouching);
			}
		}
		else if (IsAirborne())
		{
			DesiredStance = AlsStanceTags::Standing;
			Stand(CurrentCollisionShape, DeltaSeconds);
			SetStance(AlsStanceTags::Standing);
		}
	}
	else if (LocomotionAction == AlsLocomotionActionTags::Rolling && Settings->Rolling.bCrouchOnStart)
	{
		DesiredStance = AlsStanceTags::Crouching;
		Crouch(CurrentCollisionShape, DeltaSeconds);
		SetStance(AlsStanceTags::Crouching);
	}
}

bool UAlsCharacterMovementComponent::CanCrouch() const
{
	// This allows the ACharacter::Crouch() function to execute properly when bIsCrouched is true.

	// TODO Wait for https://github.com/EpicGames/UnrealEngine/pull/9558 to be merged into the engine.
	// TODO add can crouch logic
	return true;
}

void UAlsCharacterMovementComponent::SetStance(const FGameplayTag& NewStance)
{
	if (Stance != NewStance)
	{
		const auto PreviousStance{Stance};

		Stance = NewStance;
		
		RefreshGaitSettings();
		
		OnStanceChanged(PreviousStance);
	}
}

void UAlsCharacterMovementComponent::OnStanceChanged_Implementation(const FGameplayTag& PreviousStance) {}

void UAlsCharacterMovementComponent::SetDesiredGait(const FGameplayTag& NewDesiredGait)
{
	SetDesiredGait(NewDesiredGait, true);
}

void UAlsCharacterMovementComponent::SetDesiredGait(const FGameplayTag& NewDesiredGait, const bool bSendRpc)
{
	if (DesiredGait == NewDesiredGait)
	{
		return;
	}

	DesiredGait = NewDesiredGait;
}

void UAlsCharacterMovementComponent::SetGait(const FGameplayTag& NewGait)
{
	if (Gait != NewGait)
	{
		const auto PreviousGait{Gait};

		Gait = NewGait;

		OnGaitChanged(PreviousGait);
	}
}

void UAlsCharacterMovementComponent::OnGaitChanged_Implementation(const FGameplayTag& PreviousGait) {}

void UAlsCharacterMovementComponent::ApplyDesiredGait(const FGameplayTag& GaitToApply, float DeltaSeconds)
{
	if (!IsMovingOnGround())
	{
		return;
	}

	const auto CurrentMaxAllowedGait{CalculateMaxAllowedGait()};

	// Update the character max walk speed to the configured speeds based on the currently max allowed gait.

	SetMaxAllowedGait(MaxAllowedGait);

	const auto ActualGait{CalculateActualGait(MaxAllowedGait)};
	
	if (GaitToApply == AlsGaitTags::Sprinting)
	{
		SetDesiredStance(AlsStanceTags::Standing);
	}
	
	SetGait(ActualGait);
}

FGameplayTag UAlsCharacterMovementComponent::CalculateMaxAllowedGait() const
{
	// Calculate the max allowed gait. This represents the maximum gait the character is currently allowed
	// to be in and can be determined by the desired gait, the rotation mode, the stance, etc. For example,
	// if you wanted to force the character into a walking state while indoors, this could be done here.

	if (DesiredGait != AlsGaitTags::Sprinting)
	{
		return DesiredGait;
	}

	if (CanSprint())
	{
		return AlsGaitTags::Sprinting;
	}

	return AlsGaitTags::Running;
}

FGameplayTag UAlsCharacterMovementComponent::CalculateActualGait(const FGameplayTag& MaxAllowedGait) const
{
	// Calculate the new gait. This is calculated by the actual movement of the character and so it can be
	// different from the desired gait or max allowed gait. For instance, if the max allowed gait becomes
	// walking, the new gait will still be running until the character decelerates to the walking speed.
	
	if (LocomotionState.Speed < GaitSettings.GetMaxWalkSpeed() + 10.0f)
	{
		return AlsGaitTags::Walking;
	}

	if (LocomotionState.Speed < GaitSettings.GetMaxRunSpeed() + 10.0f || MaxAllowedGait != AlsGaitTags::Sprinting)
	{
		return AlsGaitTags::Running;
	}

	return AlsGaitTags::Sprinting;
}

bool UAlsCharacterMovementComponent::CanSprint() const
{
	// Determine if the character can sprint based on the rotation mode and input direction.
	// If the character is in view direction rotation mode, only allow sprinting if there is
	// input and if the input direction is aligned with the view direction within 50 degrees.

	if (!LocomotionState.bHasInput || Stance != AlsStanceTags::Standing ||
	    ((bDesiredAiming || DesiredRotationMode == AlsRotationModeTags::Aiming) && !Settings->bSprintHasPriorityOverAiming))
	{
		return false;
	}

	if (ViewMode != AlsViewModeTags::FirstPerson &&
	    (DesiredRotationMode == AlsRotationModeTags::VelocityDirection || Settings->bRotateToVelocityWhenSprinting))
	{
		return true;
	}

	static constexpr auto ViewRelativeAngleThreshold{50.0f};

	if (FMath::Abs(FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
		    LocomotionState.InputYawAngle - ViewState.Rotation.Yaw))) < ViewRelativeAngleThreshold)
	{
		return true;
	}

	return false;
}

void UAlsCharacterMovementComponent::SetOverlayMode(const FGameplayTag& NewOverlayMode)
{
	if (OverlayMode == NewOverlayMode)
	{
		return;
	}

	const auto PreviousOverlayMode{OverlayMode};

	OverlayMode = NewOverlayMode;

	OnOverlayModeChanged(PreviousOverlayMode);
}

void UAlsCharacterMovementComponent::OnOverlayModeChanged_Implementation(const FGameplayTag& PreviousOverlayMode) {}

void UAlsCharacterMovementComponent::SetLocomotionAction(const FGameplayTag& NewLocomotionAction)
{
	if (LocomotionAction != NewLocomotionAction)
	{
		const auto PreviousLocomotionAction{LocomotionAction};

		LocomotionAction = NewLocomotionAction;

		OnLocomotionActionChanged(PreviousLocomotionAction);
	}
}

void UAlsCharacterMovementComponent::OnLocomotionActionChanged_Implementation(const FGameplayTag& PreviousLocomotionAction) {}

void UAlsCharacterMovementComponent::RefreshInput(const float DeltaTime)
{
	if (GetOwnerRole() >= ROLE_AutonomousProxy)
	{
		RoundVector(ProcessedInputVector = PreProcessInputVector(GetRawInputVector()), EGMC_FloatPrecisionBlueprint::TwoDecimals);
	}

	LocomotionState.bHasInput = GetProcessedInputVector().SizeSquared() > UE_KINDA_SMALL_NUMBER;

	if (LocomotionState.bHasInput)
	{
		LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(UAlsVector::DirectionToAngleXY(GetProcessedInputVector()));
	}
}

void UAlsCharacterMovementComponent::RefreshView(const float DeltaTime)
{
	ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw);

	ViewState.Rotation = GetControllerRotation_GMC();

	// Set the yaw speed by comparing the current and previous view yaw angle, divided by
	// delta seconds. This represents the speed the camera is rotating from left to right.

	if (DeltaTime > UE_SMALL_NUMBER)
	{
		ViewState.YawSpeed = FMath::Abs(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ViewState.PreviousYawAngle)) / DeltaTime;
	}
}

void UAlsCharacterMovementComponent::SetDesiredVelocityYawAngle(const float NewVelocityYawAngle)
{
	DesiredVelocityYawAngle = NewVelocityYawAngle;
}

void UAlsCharacterMovementComponent::RefreshLocomotionLocationAndRotation()
{
	const auto& ActorTransform{GetActorTransform()};
	
	LocomotionState.Location = ActorTransform.GetLocation();
	LocomotionState.Rotation = GetBasedActorRotation();
}

void UAlsCharacterMovementComponent::RefreshLocomotionEarly()
{
	RefreshLocomotionLocationAndRotation();

	LocomotionState.PreviousVelocity = LocomotionState.Velocity;
	LocomotionState.PreviousYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
	LocomotionState.bAimingLimitAppliedThisFrame = false;
}

void UAlsCharacterMovementComponent::RefreshLocomotion()
{
	const auto bHadVelocity{LocomotionState.bHasVelocity};

	LocomotionState.Velocity = GetVelocity();

	// Determine if the character is moving by getting its speed. The speed equals the length
	// of the horizontal velocity, so it does not take vertical movement into account. If the
	// character is moving, update the last velocity rotation. This value is saved because it might
	// be useful to know the last orientation of a movement even after the character has stopped.

	LocomotionState.Speed = UE_REAL_TO_FLOAT(LocomotionState.Velocity.Size2D());

	static constexpr auto HasSpeedThreshold{1.0f};

	LocomotionState.bHasVelocity = LocomotionState.Speed >= HasSpeedThreshold;

	if (LocomotionState.bHasVelocity)
	{
		LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(UAlsVector::DirectionToAngleXY(LocomotionState.Velocity));
	}
	
	// Character is moving if has speed and current acceleration, or if the speed is greater than the moving speed threshold.

	LocomotionState.bMoving = (LocomotionState.bHasInput && LocomotionState.bHasVelocity) ||
	                          LocomotionState.Speed > Settings->MovingSpeedThreshold;
}

void UAlsCharacterMovementComponent::RefreshLocomotionLate()
{
	if (LocomotionAction.IsValid())
	{
		RefreshLocomotionLocationAndRotation();
		RefreshTargetYawAngleUsingLocomotionRotation();
	}

	LocomotionState.bResetAimingLimit = !LocomotionState.bAimingLimitAppliedThisFrame;
}

bool UAlsCharacterMovementComponent::CanJump() const
{
	return bCanJump && Stance == AlsStanceTags::Standing && IsMovingOnGround();

}

void UAlsCharacterMovementComponent::ApplyDesiredJump(bool bRequestedJump, float DeltaSeconds)
{
	if (bRequestedJump && CanJump())
	{
		AddImpulse({0., 0., JumpForce}, true);
		CharacterOwner->OnJump();
		bCanJump = false;
		bJustJumped = true;
		return;
	}
	bCanJump = true;
}

void UAlsCharacterMovementComponent::ApplyDesiredJump_Simulated(bool bPerformedJump, float DeltaSeconds)
{
	if (bPerformedJump) CharacterOwner->OnJump();
}

void UAlsCharacterMovementComponent::RefreshGroundedRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || !IsMovingOnGround())
	{
		return;
	}

	if (bHasRootMotion)
	{
		RefreshTargetYawAngleUsingLocomotionRotation();
		return;
	}

	if (RotationMode != AlsRotationModeTags::VelocityDirection)
	{
		// This prevents the actor from rotating in the last input direction after the rotation mode
		// has been changed to the velocity direction and the actor is not moving at that moment.

		LocomotionState.bRotationTowardsLastInputDirectionBlocked = true;
	}

	if (!LocomotionState.bMoving)
	{
		// Not moving.

		ApplyRotationYawSpeedAnimationCurve(DeltaTime);

		if (RefreshCustomGroundedNotMovingRotation(DeltaTime))
		{
			return;
		}

		if (RotationMode == AlsRotationModeTags::VelocityDirection)
		{
			// Rotate to the last target yaw angle when not moving (relative to the movement base or not).

			auto TargetYawAngle{
				LocomotionState.bRotationTowardsLastInputDirectionBlocked
					? LocomotionState.TargetYawAngle
					: Settings->bRotateTowardsDesiredVelocityInVelocityDirectionRotationMode
					? DesiredVelocityYawAngle
					: LocomotionState.VelocityYawAngle
			};

			static constexpr auto RotationInterpolationSpeed{12.0f};
			static constexpr auto TargetYawAngleRotationSpeed{800.0f};

			SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
			return;
		}

		if (RotationMode == AlsRotationModeTags::Aiming || ViewMode == AlsViewModeTags::FirstPerson)
		{
			RefreshGroundedAimingRotation(DeltaTime);
			return;
		}

		RefreshTargetYawAngleUsingLocomotionRotation();
		return;
	}

	// Moving.

	if (RefreshCustomGroundedMovingRotation(DeltaTime))
	{
		return;
	}

	if (RotationMode == AlsRotationModeTags::VelocityDirection &&
	    (LocomotionState.bHasInput || !LocomotionState.bRotationTowardsLastInputDirectionBlocked))
	{
		LocomotionState.bRotationTowardsLastInputDirectionBlocked = false;

		const auto TargetYawAngle{
			Settings->bRotateTowardsDesiredVelocityInVelocityDirectionRotationMode
				? DesiredVelocityYawAngle
				: LocomotionState.VelocityYawAngle
		};

		const auto RotationInterpolationSpeed{CalculateGroundedMovingRotationInterpolationSpeed()};

		static constexpr auto TargetYawAngleRotationSpeed{800.0f};

		SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::ViewDirection)
	{
		float TargetYawAngle;

		if (Gait == AlsGaitTags::Sprinting)
		{
			TargetYawAngle = LocomotionState.VelocityYawAngle;
		}
		else
		{
			TargetYawAngle = UE_REAL_TO_FLOAT(
				ViewState.Rotation.Yaw + CharacterOwner->GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawOffsetCurveName()));
		}

		const auto RotationInterpolationSpeed{CalculateGroundedMovingRotationInterpolationSpeed()};

		static constexpr auto TargetYawAngleRotationSpeed{500.0f};

		SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshGroundedAimingRotation(DeltaTime);
		return;
	}

	RefreshTargetYawAngleUsingLocomotionRotation();
}

bool UAlsCharacterMovementComponent::RefreshCustomGroundedMovingRotation(const float DeltaTime)
{
	return false;
}

bool UAlsCharacterMovementComponent::RefreshCustomGroundedNotMovingRotation(const float DeltaTime)
{
	return false;
}

void UAlsCharacterMovementComponent::RefreshGroundedAimingRotation(const float DeltaTime)
{
	auto NewActorRotation{GetActorRotation_GMC()};

	if (!LocomotionState.bHasInput && !LocomotionState.bMoving)
	{
		// Not moving.

		SetTargetYawAngle(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw));

		if (!ConstrainAimingRotation(NewActorRotation, DeltaTime, true))
		{
			return;
		}
	}
	else
	{
		// Moving.

		static constexpr auto RotationInterpolationSpeed{20.0f};
		static constexpr auto TargetYawAngleRotationSpeed{1000.0f};

		SetTargetYawAngleSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw), DeltaTime, TargetYawAngleRotationSpeed);

		NewActorRotation.Yaw = UAlsRotation::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewActorRotation.Yaw)),
		                                                           LocomotionState.SmoothTargetYawAngle,
		                                                           DeltaTime, RotationInterpolationSpeed);

		if (ConstrainAimingRotation(NewActorRotation, DeltaTime))
		{
			// Cancel the extra smooth rotation, otherwise the actor will rotate too weirdly.
			LocomotionState.SmoothTargetYawAngle = LocomotionState.TargetYawAngle;
		}
	}

	SetActorRotation_GMC(NewActorRotation, false);

	RefreshLocomotionLocationAndRotation();
}

bool UAlsCharacterMovementComponent::ConstrainAimingRotation(FRotator& ActorRotation, const float DeltaTime, const bool bApplySecondaryConstraint)
{
	// Limit the actor's rotation when aiming to prevent situations where the lower body noticeably
	// fails to keep up with the rotation of the upper body when the camera is rotating very fast.

	LocomotionState.bAimingLimitAppliedThisFrame = true;

	if (LocomotionState.bResetAimingLimit)
	{
		LocomotionState.AimingYawAngleLimit = 180.0f;
	}

	auto ViewRelativeAngle{FMath::UnwindDegrees(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ActorRotation.Yaw))};

	if (FMath::Abs(ViewRelativeAngle) <= AlsCharacterConstants::MinAimingYawAngleLimit + UE_KINDA_SMALL_NUMBER)
	{
		LocomotionState.AimingYawAngleLimit = AlsCharacterConstants::MinAimingYawAngleLimit;
		return false;
	}

	ViewRelativeAngle = UAlsRotation::RemapAngleForCounterClockwiseRotation(ViewRelativeAngle);

	// Secondary constraint. Simply increases the actor's rotation speed. Typically only used when the actor is standing still.

	if (bApplySecondaryConstraint)
	{
		static constexpr auto RotationInterpolationSpeed{20.0f};

		// Interpolate the angle only to the point where the constraints no longer apply to ensure a smoother completion of the rotation.

		const auto TargetViewRelativeAngle{
			FMath::Clamp(ViewRelativeAngle, -AlsCharacterConstants::MinAimingYawAngleLimit, AlsCharacterConstants::MinAimingYawAngleLimit)
		};

		const auto DeltaAngle{FMath::UnwindDegrees(TargetViewRelativeAngle - ViewRelativeAngle)};
		const auto InterpolationAmount{UAlsMath::ExponentialDecay(DeltaTime, RotationInterpolationSpeed)};

		ViewRelativeAngle = FMath::UnwindDegrees(ViewRelativeAngle + DeltaAngle * InterpolationAmount);
	}

	// Primary constraint. Prevents the actor from rotating beyond a certain angle relative to the camera.

	if (FMath::Abs(ViewRelativeAngle) > LocomotionState.AimingYawAngleLimit + UE_KINDA_SMALL_NUMBER)
	{
		ViewRelativeAngle = FMath::Clamp(ViewRelativeAngle, -LocomotionState.AimingYawAngleLimit, LocomotionState.AimingYawAngleLimit);
	}
	else
	{
		LocomotionState.AimingYawAngleLimit = FMath::Max(FMath::Abs(ViewRelativeAngle), AlsCharacterConstants::MinAimingYawAngleLimit);
	}

	const auto PreviousActorYawAngle{ActorRotation.Yaw};

	ActorRotation.Yaw = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ViewRelativeAngle));

	// We use UE_KINDA_SMALL_NUMBER here because even if ViewRelativeAngle hasn't
	// changed, converting it back to ActorRotation.Yaw may introduce a rounding
	// error, and FMath::IsNearlyEqual() with default arguments will return false.

	return !FMath::IsNearlyEqual(PreviousActorYawAngle, ActorRotation.Yaw, UE_KINDA_SMALL_NUMBER);
}

float UAlsCharacterMovementComponent::CalculateGroundedMovingRotationInterpolationSpeed() const
{
	// Calculate the rotation speed by using the rotation speed curve in the movement gait settings. Using
	// the curve in conjunction with the gait amount gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	const auto* InterpolationSpeedCurve{GetGaitSettings().RotationInterpolationSpeedCurve.Get()};

	static constexpr auto DefaultInterpolationSpeed{5.0f};

	const auto InterpolationSpeed{
		ALS_ENSURE(IsValid(InterpolationSpeedCurve))
			? InterpolationSpeedCurve->GetFloatValue(GetGaitAmount())
			: DefaultInterpolationSpeed
	};

	static constexpr auto MaxInterpolationSpeedMultiplier{3.0f};
	static constexpr auto ReferenceViewYawSpeed{300.0f};

	return InterpolationSpeed * UAlsMath::LerpClamped(1.0f, MaxInterpolationSpeedMultiplier,
	                                                  ViewState.YawSpeed / ReferenceViewYawSpeed);
}

void UAlsCharacterMovementComponent::ApplyRotationYawSpeedAnimationCurve(const float DeltaTime)
{
	const auto DeltaYawAngle{CharacterOwner->GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawSpeedCurveName()) * DeltaTime};
	if (FMath::Abs(DeltaYawAngle) > UE_SMALL_NUMBER)
	{
		auto NewRotation{GetActorRotation_GMC()};
		NewRotation.Yaw += DeltaYawAngle;

		SetActorRotation_GMC(NewRotation, false);

		RefreshLocomotionLocationAndRotation();
		RefreshTargetYawAngleUsingLocomotionRotation();
	}
}

void UAlsCharacterMovementComponent::RefreshInAirRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || !IsAirborne())
	{
		return;
	}

	if (RefreshCustomInAirRotation(DeltaTime))
	{
		return;
	}

	static constexpr auto RotationInterpolationSpeed{5.0f};

	if (RotationMode == AlsRotationModeTags::VelocityDirection || RotationMode == AlsRotationModeTags::ViewDirection)
	{
		switch (Settings->InAirRotationMode)
		{
			case EAlsInAirRotationMode::RotateToVelocityOnJump:
				if (LocomotionState.bMoving)
				{
					SetRotationSmooth(LocomotionState.VelocityYawAngle, DeltaTime, RotationInterpolationSpeed);
				}
				else
				{
					RefreshTargetYawAngleUsingLocomotionRotation();
				}
				break;

			case EAlsInAirRotationMode::KeepRelativeRotation:
				SetRotationSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - LocomotionState.ViewRelativeTargetYawAngle),
				                  DeltaTime, RotationInterpolationSpeed);
				break;

			default:
				RefreshTargetYawAngleUsingLocomotionRotation();
				break;
		}
	}
	else if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshInAirAimingRotation(DeltaTime);
	}
	else
	{
		RefreshTargetYawAngleUsingLocomotionRotation();
	}
}

bool UAlsCharacterMovementComponent::RefreshCustomInAirRotation(const float DeltaTime)
{
	return false;
}

void UAlsCharacterMovementComponent::RefreshInAirAimingRotation(const float DeltaTime)
{
	static constexpr auto RotationInterpolationSpeed{15.0f};

	SetTargetYawAngle(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw));

	auto NewRotation{GetActorRotation_GMC()};
	NewRotation.Yaw = UAlsRotation::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                      LocomotionState.SmoothTargetYawAngle, DeltaTime, RotationInterpolationSpeed);

	ConstrainAimingRotation(NewRotation, DeltaTime);

	SetActorRotation_GMC(NewRotation, false);

	RefreshLocomotionLocationAndRotation();
}

void UAlsCharacterMovementComponent::SetRotationSmooth(const float TargetYawAngle, const float DeltaTime, const float InterpolationSpeed)
{
	SetTargetYawAngle(TargetYawAngle);

	auto NewRotation{GetActorRotation_GMC()};
	NewRotation.Yaw = UAlsRotation::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                      LocomotionState.SmoothTargetYawAngle, DeltaTime, InterpolationSpeed);

	SetActorRotation_GMC(NewRotation, false);

	RefreshLocomotionLocationAndRotation();
}

void UAlsCharacterMovementComponent::SetRotationExtraSmooth(const float TargetYawAngle, const float DeltaTime,
                                           const float InterpolationSpeed, const float TargetYawAngleRotationSpeed)
{
	SetTargetYawAngleSmooth(TargetYawAngle, DeltaTime, TargetYawAngleRotationSpeed);

	auto NewRotation{GetActorRotation_GMC()};
	NewRotation.Yaw = UAlsRotation::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                      LocomotionState.SmoothTargetYawAngle, DeltaTime, InterpolationSpeed);

	SetActorRotation_GMC(NewRotation, false);

	RefreshLocomotionLocationAndRotation();
}

void UAlsCharacterMovementComponent::SetRotationInstant(const float TargetYawAngle)
{
	SetTargetYawAngle(TargetYawAngle);

	auto NewRotation{GetActorRotation_GMC()};
	NewRotation.Yaw = TargetYawAngle;

	SetActorRotation_GMC(NewRotation, true);

	RefreshLocomotionLocationAndRotation();
}

void UAlsCharacterMovementComponent::RefreshTargetYawAngleUsingLocomotionRotation()
{
	SetTargetYawAngle(UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw));
}

void UAlsCharacterMovementComponent::SetTargetYawAngle(const float TargetYawAngle)
{
	LocomotionState.TargetYawAngle = FMath::UnwindDegrees(TargetYawAngle);

	LocomotionState.SmoothTargetYawAngle = LocomotionState.TargetYawAngle;

	RefreshViewRelativeTargetYawAngle();
}

void UAlsCharacterMovementComponent::SetTargetYawAngleSmooth(const float TargetYawAngle, const float DeltaTime, const float RotationSpeed)
{
	LocomotionState.TargetYawAngle = FMath::UnwindDegrees(TargetYawAngle);

	LocomotionState.SmoothTargetYawAngle = UAlsRotation::InterpolateAngleConstant(
		LocomotionState.SmoothTargetYawAngle, LocomotionState.TargetYawAngle, DeltaTime, RotationSpeed);

	RefreshViewRelativeTargetYawAngle();
}

void UAlsCharacterMovementComponent::RefreshViewRelativeTargetYawAngle()
{
	LocomotionState.ViewRelativeTargetYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
		ViewState.Rotation.Yaw - LocomotionState.TargetYawAngle));
}
