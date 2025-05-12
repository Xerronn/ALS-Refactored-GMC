// Fill out your copyright notice in the Description page of Project Settings.

#include "AlsCharacterMovementComponent.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacter.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Settings/AlsCharacterSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsDebugUtility.h"
#include "Utility/AlsLog.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsMontageUtility.h"
#include "Utility/AlsRotation.h"
#include "Utility/AlsVector.h"

bool UAlsCharacterMovementComponent::CanJump() const
{
	return bCanJump && Stance == AlsStanceTags::Standing && IsMovingOnGround() && !LocomotionAction.IsValid();

}

EGMC_CollisionShape UAlsCharacterMovementComponent::InterpToSphereAndSwitchCollisionShape(
  EGMC_CollisionShape CurrentShape,
  float SphereRadius,
  float DeltaSeconds
)
{
	const EGMC_CollisionShape TargetShape =
	  CurrentShape == EGMC_CollisionShape::VerticalCapsule ? EGMC_CollisionShape::HorizontalCapsule : EGMC_CollisionShape::VerticalCapsule;

	const FVector Extent = GetRootCollisionExtent(true);
	float CurrentRadius = Extent.X;
	float CurrentHalfHeight = Extent.Z;

	CurrentHalfHeight -= TargetShape == EGMC_CollisionShape::HorizontalCapsule ?
	  LerpRootCollisionHalfHeight(CurrentRadius, ChangeStanceSpeed, 0.99f, DeltaSeconds, true,EGMC_AdjustDirection::Down) :
	  LerpRootCollisionWidth(CurrentRadius, ChangeStanceSpeed, 0.99f, DeltaSeconds);

	if (FMath::IsNearlyEqual(CurrentHalfHeight, CurrentRadius, 1.f))
	{
		SetRootCollisionShape(TargetShape, FVector{SphereRadius}, true);
		return TargetShape;
	}

	return CurrentShape;
}

void UAlsCharacterMovementComponent::MaintainMeshOffset()
{
	if (!SkeletalMesh || SkeletalMesh->IsAnySimulatingPhysics())
	{
		return;
	}
	const FVector CurrentRelativeLocation = SkeletalMesh->GetRelativeLocation();
	SkeletalMesh->SetRelativeLocation({CurrentRelativeLocation.X, CurrentRelativeLocation.Y, -GetRootCollisionHalfHeight(true)});
}

void UAlsCharacterMovementComponent::MaintainMeshOffsetSimulated()
{
	if (!SkeletalMesh || SkeletalMesh->IsAnySimulatingPhysics())
	{
		return;
	}

	if (!IsExtrapolating())
	{
		const int32 TargetIdx	= GetSmoothingTargetIdx();
		const int32 StartIdx	= TargetIdx - 1;
		const double SmoothingTime = GetSmoothingTime();
		if(SmoothingTime < 0. || !IsValidMoveHistoryIndex(StartIdx) || !IsValidMoveHistoryIndex(TargetIdx))
		{
			return;
		}
	
		const auto& StartState = MoveHistory[StartIdx].OutputState;
		const auto& TargetState = MoveHistory[TargetIdx].OutputState;
	
		const uint8 StartShape = StartState.UnsignedInt4.Read(BI_CurrentRootCollisionShape);
		const uint8 TargetShape = TargetState.UnsignedInt4.Read(BI_CurrentRootCollisionShape);
	
		if (StartShape != TargetShape)
		{
			const double StartTime = MoveHistory[StartIdx].MetaData.Timestamp;
			const double TargetTime = MoveHistory[TargetIdx].MetaData.Timestamp;
			const float Alpha = FMath::Clamp((SmoothingTime - StartTime) / FMath::Max(TargetTime - StartTime, (double)MIN_DELTA_TIME), 0., 1.);
	
			auto& NoSmoothState = const_cast<FGMC_PawnState&>(Alpha < 0.5 ? StartState : TargetState);
			ProcessSyncData(NoSmoothState, {DataOp::Apply}, AliasData, bUseRelativeValuesForSimulation, this);
		}
	}
	
	MaintainMeshOffset();
}

//stances
void UAlsCharacterMovementComponent::Stand(EGMC_CollisionShape CurrentCollisionShape, float DeltaSeconds)
{
	if (CurrentCollisionShape == EGMC_CollisionShape::HorizontalCapsule)
	{
		CurrentCollisionShape = InterpToSphereAndSwitchCollisionShape(CurrentCollisionShape, DefaultRadius, DeltaSeconds);
	}

	if (CurrentCollisionShape == EGMC_CollisionShape::VerticalCapsule)
	{
		const float ChangeAmount = LerpRootCollisionHalfHeight(StandingHalfHeight, ChangeStanceSpeed, 0.99f, DeltaSeconds, true, EGMC_AdjustDirection::Up);
		bChangingStance = ChangeAmount > 0.f;
		const bool bMovedUp = ChangeAmount > 0.f;
		if (bMovedUp)
		{
			// The client can get stuck with too large timesteps in this case when blocked so don't combine.
			CL_DoNotCombineNextMove();
		}
	}
}

void UAlsCharacterMovementComponent::Crouch(EGMC_CollisionShape CurrentCollisionShape, float DeltaSeconds)
{
	if (CurrentCollisionShape == EGMC_CollisionShape::HorizontalCapsule)
	{
		CurrentCollisionShape = InterpToSphereAndSwitchCollisionShape(CurrentCollisionShape, DefaultRadius, DeltaSeconds);
	}

	if (CurrentCollisionShape == EGMC_CollisionShape::VerticalCapsule)
	{
		const auto AdjustDirection = GetRootCollisionHalfHeight(true) < CrouchedHalfHeight ? EGMC_AdjustDirection::Up : EGMC_AdjustDirection::Down;
		const float ChangeAmount = LerpRootCollisionHalfHeight(CrouchedHalfHeight, ChangeStanceSpeed, 0.99f, DeltaSeconds, true, AdjustDirection);
		bChangingStance = ChangeAmount > 0.f;
		const bool bMovedUp = ChangeAmount > 0.f && AdjustDirection == EGMC_AdjustDirection::Up;
		if (bMovedUp)
		{
			// The client can get stuck with too large timesteps in this case when blocked so don't combine.
			CL_DoNotCombineNextMove();
		}
	}
}

bool UAlsCharacterMovementComponent::IsRagdollingAllowedToStart() const
{
	return LocomotionAction != AlsLocomotionActionTags::Ragdolling;
}

void UAlsCharacterMovementComponent::OnRagdollingStarted_Implementation() {}

void UAlsCharacterMovementComponent::ToggleRagdolling(bool bActive)
{
	if (bActive)
	{
		RagdollingState.TargetLocation = FVector::ZeroVector;
		RagdollingState.TargetRotation = FRotator::ZeroRotator;
		RagdollingState.LastTickLocation = GetActorLocation_GMC();

		HaltMovement();
		bSmoothRemoteListenServerPawn = false;
		SetLocomotionAction(AlsLocomotionActionTags::Ragdolling);
		OnRagdollingStarted();
	}
	else
	{
		bool bGrounded;
		const auto NewActorLocation{RagdollTraceGround(bGrounded)};

		const auto bRagdollFacingUpward{FRotator::NormalizeAxis(RagdollingState.TargetRotation.Roll) <= 0.0f};

		auto NewActorRotation{GetActorRotation_GMC()};
		NewActorRotation.Yaw = bRagdollFacingUpward ? RagdollingState.TargetRotation.Yaw - 180.0f : RagdollingState.TargetRotation.Yaw;
	
		SetActorLocationAndRotation_GMC(NewActorLocation, NewActorRotation, false);

		if (bGrounded)
		{
			SetLocomotionAction(AlsLocomotionActionTags::GettingUp);
			PlayMontage_Blocking(CharacterOwner->GetMesh(), MontageTracker, SelectGetUpMontage(bRagdollFacingUpward), 0.0f, 1.0f);
		}
		else
		{
			SetLocomotionAction(FGameplayTag::EmptyTag);
		}
		
		bSmoothRemoteListenServerPawn = true;
		OnRagdollingEnded();

	}
	
	RagdollingState.bFirstTick = bActive;
	
	bEnablePhysicsInteraction = !bActive;
	RagdollingState.bResetMesh = !bActive;
}

FVector UAlsCharacterMovementComponent::RagdollTraceGround(bool& bGrounded) const
{
	auto RagdollLocation{!RagdollingState.TargetLocation.IsZero() ? RagdollingState.TargetLocation : GetActorLocation_GMC()};
	
	// We use a sphere sweep instead of a simple line trace to keep capsule
	// movement consistent between ragdolling and regular character movement.

	const FVector Extent = GetRootCollisionExtent(true);
	const auto CapsuleRadius{Extent.X};
	const auto CapsuleHalfHeight{Extent.Z};
	
	const FVector TraceStart{RagdollLocation.X, RagdollLocation.Y, RagdollLocation.Z + 2.0f * CapsuleRadius};
	const FVector TraceEnd{RagdollLocation.X, RagdollLocation.Y, RagdollLocation.Z - CapsuleHalfHeight + CapsuleRadius};
	
	FCollisionQueryParams QueryParameters{__FUNCTION__, false, CharacterOwner};
	FCollisionResponseParams ResponseParameters;
	
	FHitResult Hit;
	bGrounded = GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity,
	                                             ECC_WorldStatic, FCollisionShape::MakeSphere(CapsuleRadius),
	                                             QueryParameters, ResponseParameters);
	
	return FVector{
		RagdollLocation.X, RagdollLocation.Y,
		bGrounded
			? Hit.Location.Z + CapsuleHalfHeight - CapsuleRadius + 1.9f
			: RagdollLocation.Z
	};
}

bool UAlsCharacterMovementComponent::IsRagdollingAllowedToStop() const
{
	return LocomotionAction == AlsLocomotionActionTags::Ragdolling;
}

void UAlsCharacterMovementComponent::OnRagdollingEnded_Implementation() {}

UAnimMontage* UAlsCharacterMovementComponent::SelectGetUpMontage_Implementation(const bool bRagdollFacingUpward)
{
	return bRagdollFacingUpward ? Settings->Ragdolling.GetUpBackMontage : Settings->Ragdolling.GetUpFrontMontage;
}

bool UAlsCharacterMovementComponent::IsRollingAllowedToStart() const
{
	return !LocomotionAction.IsValid() && IsMovingOnGround();
}

void UAlsCharacterMovementComponent::StartRolling(const float PlayRate)
{
	auto* Montage{SelectRollMontage()};

	if (!ALS_ENSURE(IsValid(Montage)) || !IsRollingAllowedToStart())
	{
		return;
	}
	SetLocomotionAction(AlsLocomotionActionTags::Rolling);
	PlayMontage_Blocking(CharacterOwner->GetMesh(), MontageTracker, Montage, 0.0f, PlayRate, true);
	
	if (Settings->Rolling.bCrouchOnStart)
	{
		Crouch(GetRootCollisionShape(), 100000.f);

		IsSimulatedPawn() ? MaintainMeshOffsetSimulated() : MaintainMeshOffset();
	}
}

UAnimMontage* UAlsCharacterMovementComponent::SelectRollMontage_Implementation()
{
	return Settings->Rolling.Montage;
}


bool UAlsCharacterMovementComponent::StartMantlingGrounded()
{
	return IsMovingOnGround() &&
	       CheckMantle(Settings->Mantling.GroundedTrace);
}

bool UAlsCharacterMovementComponent::StartMantlingInAir()
{
	return IsAirborne() &&
	       CheckMantle(Settings->Mantling.InAirTrace);
}

bool UAlsCharacterMovementComponent::IsMantlingAllowedToStart_Implementation() const
{
	return !LocomotionAction.IsValid() && Settings->Mantling.bAllowMantling;
}

bool UAlsCharacterMovementComponent::CheckMantle(const FAlsMantlingTraceSettings& TraceSettings)
{
	if (!IsMantlingAllowedToStart())
	{
		return false;
	}

	const auto ActorLocation{GetActorLocation()};
	const auto ActorYawAngle{UE_REAL_TO_FLOAT(FMath::UnwindDegrees(GetActorRotation_GMC().Yaw))};

	float ForwardTraceAngle;
	if (LocomotionState.bHasVelocity)
	{
		ForwardTraceAngle = LocomotionState.bHasInput
			                    ? LocomotionState.VelocityYawAngle +
			                      FMath::ClampAngle(LocomotionState.InputYawAngle - LocomotionState.VelocityYawAngle,
			                                        -Settings->Mantling.MaxReachAngle, Settings->Mantling.MaxReachAngle)
			                    : LocomotionState.VelocityYawAngle;
	}
	else
	{
		ForwardTraceAngle = LocomotionState.bHasInput ? LocomotionState.InputYawAngle : ActorYawAngle;
	}

	const auto ForwardTraceDeltaAngle{FMath::UnwindDegrees(ForwardTraceAngle - ActorYawAngle)};
	if (FMath::Abs(ForwardTraceDeltaAngle) > Settings->Mantling.TraceAngleThreshold)
	{
		return false;
	}

	const auto ForwardTraceDirection{
		UAlsVector::AngleToDirectionXY(
			ActorYawAngle + FMath::ClampAngle(ForwardTraceDeltaAngle, -Settings->Mantling.MaxReachAngle, Settings->Mantling.MaxReachAngle))
	};

#if ENABLE_DRAW_DEBUG
	const auto bDisplayDebug{UAlsDebugUtility::ShouldDisplayDebugForActor(CharacterOwner, UAlsConstants::MantlingDebugDisplayName())};
#endif

	const TObjectPtr<UCapsuleComponent> Capsule = CharacterOwner->GetCapsuleComponent();

	const auto CapsuleScale{Capsule->GetComponentScale().Z};
	const auto CapsuleRadius{Capsule->GetScaledCapsuleRadius()};
	const auto CapsuleHalfHeight{Capsule->GetScaledCapsuleHalfHeight()};

	const FVector CapsuleBottomLocation{ActorLocation.X, ActorLocation.Y, ActorLocation.Z - CapsuleHalfHeight};

	const auto TraceCapsuleRadius{CapsuleRadius - 1.0f};

	const auto LedgeHeightDelta{UE_REAL_TO_FLOAT((TraceSettings.LedgeHeight.GetMax() - TraceSettings.LedgeHeight.GetMin()) * CapsuleScale)};

	// Trace forward to find an object the character cannot walk on.

	static const FName ForwardTraceTag{FString::Printf(TEXT("%hs (Forward Trace)"), __FUNCTION__)};

	auto ForwardTraceStart{CapsuleBottomLocation - ForwardTraceDirection * CapsuleRadius};
	ForwardTraceStart.Z += (TraceSettings.LedgeHeight.X + TraceSettings.LedgeHeight.Y) *
		0.5f * CapsuleScale - UCharacterMovementComponent::MAX_FLOOR_DIST;

	auto ForwardTraceEnd{ForwardTraceStart + ForwardTraceDirection * (CapsuleRadius + (TraceSettings.ReachDistance + 1.0f) * CapsuleScale)};

	const auto ForwardTraceCapsuleHalfHeight{LedgeHeightDelta * 0.5f};

	FHitResult ForwardTraceHit;
	GetWorld()->SweepSingleByChannel(ForwardTraceHit, ForwardTraceStart, ForwardTraceEnd,
	                                 FQuat::Identity, Settings->Mantling.MantlingTraceChannel,
	                                 FCollisionShape::MakeCapsule(TraceCapsuleRadius, ForwardTraceCapsuleHalfHeight),
	                                 {ForwardTraceTag, false, CharacterOwner}, Settings->Mantling.MantlingTraceResponses);

	auto* TargetPrimitive{ForwardTraceHit.GetComponent()};

	if (!ForwardTraceHit.IsValidBlockingHit() ||
	    !IsValid(TargetPrimitive) ||
	    TargetPrimitive->GetComponentVelocity().SizeSquared() > FMath::Square(Settings->Mantling.TargetPrimitiveSpeedThreshold) ||
	    !TargetPrimitive->CanCharacterStepUp(CharacterOwner) ||
	    HitWalkableFloor(ForwardTraceHit))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsDebugUtility::DrawSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, false, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);
		}
#endif

		return false;
	}

	const auto TargetDirection{-ForwardTraceHit.ImpactNormal.GetSafeNormal2D()};

	// Trace downward from the first trace's impact point and determine if the hit location is walkable.

	static const FName DownwardTraceTag{FString::Printf(TEXT("%hs (Downward Trace)"), __FUNCTION__)};

	const FVector2D TargetLocationOffset{TargetDirection * (TraceSettings.TargetLocationOffset * CapsuleScale)};

	const FVector DownwardTraceStart{
		ForwardTraceHit.ImpactPoint.X + TargetLocationOffset.X,
		ForwardTraceHit.ImpactPoint.Y + TargetLocationOffset.Y,
		CapsuleBottomLocation.Z + LedgeHeightDelta + 2.5f * TraceCapsuleRadius + UCharacterMovementComponent::MIN_FLOOR_DIST
	};

	const FVector DownwardTraceEnd{
		DownwardTraceStart.X,
		DownwardTraceStart.Y,
		CapsuleBottomLocation.Z +
		TraceSettings.LedgeHeight.GetMin() * CapsuleScale + TraceCapsuleRadius - UCharacterMovementComponent::MAX_FLOOR_DIST
	};

	FHitResult DownwardTraceHit;
	GetWorld()->SweepSingleByChannel(DownwardTraceHit, DownwardTraceStart, DownwardTraceEnd, FQuat::Identity,
	                                 Settings->Mantling.MantlingTraceChannel, FCollisionShape::MakeSphere(TraceCapsuleRadius),
	                                 {DownwardTraceTag, false, CharacterOwner}, Settings->Mantling.MantlingTraceResponses);

	const auto SlopeAngleCos{UE_REAL_TO_FLOAT(DownwardTraceHit.ImpactNormal.Z)};

	// The approximate slope angle is used in situations where the normal slope angle cannot convey
	// the true nature of the surface slope, for example, for a 45 degree staircase the slope
	// angle will always be 90 degrees, while the approximate slope angle will be ~45 degrees.

	auto ApproximateSlopeNormal{DownwardTraceHit.Location - DownwardTraceHit.ImpactPoint};
	ApproximateSlopeNormal.Normalize();

	const auto ApproximateSlopeAngleCos{UE_REAL_TO_FLOAT(ApproximateSlopeNormal.Z)};

	if (SlopeAngleCos < Settings->Mantling.SlopeAngleThresholdCos ||
	    ApproximateSlopeAngleCos < Settings->Mantling.SlopeAngleThresholdCos ||
	    !HitWalkableFloor(DownwardTraceHit))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsDebugUtility::DrawSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);

			UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd, TraceCapsuleRadius,
			                                        false, DownwardTraceHit, {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f},
			                                        TraceSettings.bDrawFailedTraces ? 7.5f : 0.0f);
		}
#endif

		return false;
	}

	// Check that there is enough free space for the capsule at the target location.

	static const FName TargetLocationTraceTag{FString::Printf(TEXT("%hs (Target Location Overlap)"), __FUNCTION__)};

	const FVector TargetLocation{
		DownwardTraceHit.Location.X,
		DownwardTraceHit.Location.Y,
		DownwardTraceHit.ImpactPoint.Z + UCharacterMovementComponent::MIN_FLOOR_DIST
	};

	const FVector TargetCapsuleLocation{TargetLocation.X, TargetLocation.Y, TargetLocation.Z + CapsuleHalfHeight};

	if (GetWorld()->OverlapBlockingTestByChannel(TargetCapsuleLocation, FQuat::Identity, Settings->Mantling.MantlingTraceChannel,
	                                             FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
	                                             {TargetLocationTraceTag, false, CharacterOwner}, Settings->Mantling.MantlingTraceResponses))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsDebugUtility::DrawSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);

			UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd, TraceCapsuleRadius,
			                                        false, DownwardTraceHit, {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f},
			                                        TraceSettings.bDrawFailedTraces ? 7.5f : 0.0f);

			DrawDebugCapsule(GetWorld(), TargetCapsuleLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity,
			                 FColor::Red, false, TraceSettings.bDrawFailedTraces ? 10.0f : 0.0f);
		}
#endif

		return false;
	}

	// Perform additional overlap at the approximate start location to
	// ensure there are no vertical obstacles on the path, such as a ceiling.

	static const FName StartLocationTraceTag{FString::Printf(TEXT("%hs (Start Location Overlap)"), __FUNCTION__)};

	const FVector2D StartLocationOffset{TargetDirection * (TraceSettings.StartLocationOffset * CapsuleScale)};

	const FVector StartLocation{
		ForwardTraceHit.ImpactPoint.X - StartLocationOffset.X,
		ForwardTraceHit.ImpactPoint.Y - StartLocationOffset.Y,
		(DownwardTraceHit.Location.Z + DownwardTraceEnd.Z) * 0.5f
	};

	const auto StartLocationTraceCapsuleHalfHeight{
		UE_REAL_TO_FLOAT(DownwardTraceHit.Location.Z - DownwardTraceEnd.Z) * 0.5f + TraceCapsuleRadius
	};

	if (GetWorld()->OverlapBlockingTestByChannel(StartLocation, FQuat::Identity, Settings->Mantling.MantlingTraceChannel,
	                                             FCollisionShape::MakeCapsule(TraceCapsuleRadius, StartLocationTraceCapsuleHalfHeight),
	                                             {StartLocationTraceTag, false, CharacterOwner}, Settings->Mantling.MantlingTraceResponses))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsDebugUtility::DrawSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit,
			                                                    {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);

			UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd, TraceCapsuleRadius,
			                                        false, DownwardTraceHit, {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f},
			                                        TraceSettings.bDrawFailedTraces ? 7.5f : 0.0f);

			DrawDebugCapsule(GetWorld(), StartLocation, StartLocationTraceCapsuleHalfHeight, TraceCapsuleRadius, FQuat::Identity,
			                 FLinearColor{1.0f, 0.5f, 0.0f}.ToFColor(true), false, TraceSettings.bDrawFailedTraces ? 10.0f : 0.0f);
		}
#endif

		return false;
	}

#if ENABLE_DRAW_DEBUG
	if (bDisplayDebug)
	{
		UAlsDebugUtility::DrawSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
		                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit,
		                                                    {0.0f, 0.25f, 1.0f}, {0.0f, 0.75f, 1.0f}, 5.0f);

		UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd,
		                                        TraceCapsuleRadius, true, DownwardTraceHit,
		                                        {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f}, 7.5f);
	}
#endif

	const auto TargetRotation{TargetDirection.ToOrientationQuat()};

	FAlsMantlingParameters Parameters;

	Parameters.TargetPrimitive = TargetPrimitive;
	Parameters.MantlingHeight = UE_REAL_TO_FLOAT((TargetLocation.Z - CapsuleBottomLocation.Z) / CapsuleScale);

	// Determine the mantling type by checking the movement mode and mantling height.

	Parameters.MantlingType = !IsMovingOnGround()
		                          ? EAlsMantlingType::InAir
		                          : Parameters.MantlingHeight > Settings->Mantling.MantlingHighHeightThreshold
		                          ? EAlsMantlingType::High
		                          : EAlsMantlingType::Low;


	Parameters.TargetLocation = TargetLocation;
	Parameters.TargetRotation = TargetRotation.Rotator().GetNormalized();
	
	StartMantling(Parameters);
	
	return true;
}

void UAlsCharacterMovementComponent::StartMantling(const FAlsMantlingParameters& Parameters)
{
	if (!IsMantlingAllowedToStart())
	{
		return;
	}

	auto* MantlingSettings{SelectMantlingSettings(Parameters.MantlingType)};

	if (!ALS_ENSURE(IsValid(MantlingSettings)) || !ALS_ENSURE(IsValid(MantlingSettings->Montage)))
	{
		return;
	}

	const auto StartTime{CalculateMantlingStartTime(MantlingSettings, Parameters.MantlingHeight)};
	const auto PlayRate{MantlingSettings->Montage->RateScale};

	const auto TargetAnimationLocation{UAlsMontageUtility::ExtractLastRootTransformFromMontage(MantlingSettings->Montage).GetLocation()};

	if (FMath::IsNearlyZero(TargetAnimationLocation.Z))
	{
		UE_LOG(LogAls, Warning, TEXT("Can't start mantling! The %s animation montage has incorrect root motion,")
		       TEXT(" the final vertical location of the character must be non-zero!"), *MantlingSettings->Montage->GetName());
		return;
	}
	const auto TargetTransform{FTransform{Parameters.TargetRotation, Parameters.TargetLocation}};

	const auto ActorFeetLocationOffset{GetActorFeetLocation() - TargetTransform.GetLocation()};
	const auto ActorRotationOffset{TargetTransform.GetRotation().Inverse() * GetActorQuat_GMC()};
	
	MantlingState.MantlingType = static_cast<uint8>(Parameters.MantlingType);
	MantlingState.TargetPrimitive = Parameters.TargetPrimitive;
	MantlingState.TargetLocation = Parameters.TargetLocation;
	MantlingState.TargetRotation = Parameters.TargetRotation;
	MantlingState.ActorFeetLocationOffset = ActorFeetLocationOffset;
	MantlingState.ActorRotationOffset = ActorRotationOffset.Rotator();
	MantlingState.TargetAnimationLocation = TargetAnimationLocation;
	MantlingState.MontageStartTime = StartTime;

	// Clear the character movement mode and set the locomotion action to mantling.

	SetMovementMode(GetMantlingMode());
	
	// Play the animation montage if valid.
	PlayMontage_Blocking(SkeletalMesh, MontageTracker, MantlingSettings->Montage, StartTime, PlayRate);
	SetLocomotionAction(AlsLocomotionActionTags::Mantling);

	OnMantlingStarted(Parameters);
}

UAlsMantlingSettings* UAlsCharacterMovementComponent::SelectMantlingSettings_Implementation(EAlsMantlingType MantlingType)
{
	return nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
float UAlsCharacterMovementComponent::CalculateMantlingStartTime(const UAlsMantlingSettings* MantlingSettings, const float MantlingHeight) const
{
	if (!MantlingSettings->bAutoCalculateStartTime)
	{
		return FMath::GetMappedRangeValueClamped(MantlingSettings->StartTimeReferenceHeight, MantlingSettings->StartTime, MantlingHeight);
	}

	// https://landelare.github.io/2022/05/15/climbing-with-root-motion.html

	const auto* Montage{MantlingSettings->Montage.Get()};
	if (!IsValid(Montage))
	{
		return 0.0f;
	}

	const auto MontageFrameRate{1.0f / Montage->GetSamplingFrameRate().AsDecimal()};

	auto SearchStartTime{0.0f};
	auto SearchEndTime{Montage->GetPlayLength()};

	const auto SearchStartLocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, SearchStartTime).GetTranslation().Z};
	const auto SearchEndLocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, SearchEndTime).GetTranslation().Z};

	// Find the vertical distance the character has already moved.

	const auto TargetLocationZ{FMath::Max(0.0f, SearchEndLocationZ - MantlingHeight)};

	// Perform a binary search to find the time when the character is at the target vertical distance.

	static constexpr auto MaxLocationSearchTolerance{1.0f};

	if (FMath::IsNearlyEqual(SearchStartLocationZ, TargetLocationZ, MaxLocationSearchTolerance))
	{
		return SearchStartTime;
	}

	while (true)
	{
		const auto Time{(SearchStartTime + SearchEndTime) * 0.5f};
		const auto LocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, Time).GetTranslation().Z};

		// Stop the search if a close enough location has been found or if
		// the search interval is less than the animation montage frame rate.

		if (FMath::IsNearlyEqual(LocationZ, TargetLocationZ, MaxLocationSearchTolerance) ||
		    SearchEndTime - SearchStartTime <= MontageFrameRate)
		{
			return Time;
		}

		if (LocationZ < TargetLocationZ)
		{
			SearchStartTime = Time;
		}
		else
		{
			SearchEndTime = Time;
		}
	}
}

void UAlsCharacterMovementComponent::OnMantlingStarted_Implementation(const FAlsMantlingParameters& Parameters) {}

void UAlsCharacterMovementComponent::RefreshMantling(float DeltaTime)
{
	if (LocomotionAction != AlsLocomotionActionTags::Mantling)
	{
		return;
	}

	MantlingState.MantlingTimer += DeltaTime;

	auto* MantlingSettings{SelectMantlingSettings(static_cast<EAlsMantlingType>(MantlingState.MantlingType))};

	auto* Montage{MantlingSettings->Montage.Get()};
	const auto MontageTime{MantlingState.MontageStartTime + MantlingState.MantlingTimer * Montage->RateScale};
	
	SetMontagePosition(SkeletalMesh, MontageTracker, FMath::Max(0.0f, MontageTime - DeltaTime));
	
	auto TargetTransform{FTransform{MantlingState.TargetRotation, MantlingState.TargetLocation}};

	// Remove the pitch and roll components of the rotation so that the actor's Z axis is always aligned with the gravity direction.

	const auto Twist{UAlsRotation::GetTwist(TargetTransform.GetRotation(), -FVector(0.f,0.f,-1.f))};
	TargetTransform.SetRotation(Twist);

	auto BlendInAmount{1.0f};

	const auto& MontageBlendIn{Montage->BlendIn};
	if (MontageBlendIn.GetBlendTime() > 0.0f)
	{
		BlendInAmount = FAlphaBlend::AlphaToBlendOption(MantlingState.MantlingTimer / MontageBlendIn.GetBlendTime(),
		                                                MontageBlendIn.GetBlendOption(), MontageBlendIn.GetCustomCurve());
	}

	const auto CurrentAnimationLocation{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, MontageTime).GetLocation()};

	// The target animation location is expected to be non-zero, so it's safe to divide by it here.

	const auto InterpolationAmount{UE_REAL_TO_FLOAT(CurrentAnimationLocation.Z / MantlingState.TargetAnimationLocation.Z)};
	
	if (!FAnimWeight::IsFullWeight(BlendInAmount * InterpolationAmount))
	{
		// Calculate the target animation location offset. This is the offset to
		// the location where the animation ends relative to the target transform.

		auto TargetAnimationLocationOffset{TargetTransform.GetUnitAxis(EAxis::X) * -MantlingState.TargetAnimationLocation.Y};
		TargetAnimationLocationOffset.Z = -MantlingState.TargetAnimationLocation.Z;
		TargetAnimationLocationOffset *= CharacterOwner->GetMesh()->GetComponentScale().Z;

		// Blend into the animation offset and the final offset at the same time.
		// Horizontal and vertical blends use different correction amounts.

		auto HorizontalCorrectionAmount{1.0f};
		auto VerticalCorrectionAmount{1.0f};

		if (IsValid(MantlingSettings->HorizontalCorrectionCurve))
		{
			HorizontalCorrectionAmount = MantlingSettings->HorizontalCorrectionCurve->GetFloatValue(MontageTime);
		}

		if (IsValid(MantlingSettings->VerticalCorrectionCurve))
		{
			VerticalCorrectionAmount = MantlingSettings->VerticalCorrectionCurve->GetFloatValue(MontageTime);
		}
		
		FVector LocationOffset{
			FMath::Lerp(MantlingState.ActorFeetLocationOffset.X, TargetAnimationLocationOffset.X, HorizontalCorrectionAmount),
			FMath::Lerp(MantlingState.ActorFeetLocationOffset.Y, TargetAnimationLocationOffset.Y, HorizontalCorrectionAmount),
			FMath::Lerp(MantlingState.ActorFeetLocationOffset.Z, TargetAnimationLocationOffset.Z, VerticalCorrectionAmount)
		};

		LocationOffset = FMath::Lerp(MantlingState.ActorFeetLocationOffset, LocationOffset * (1.0f - InterpolationAmount), BlendInAmount);

		// The actor's rotation offset must be normalized for this code block to work properly.

		const auto RotationOffset{
			MantlingState.ActorRotationOffset *
			FMath::Lerp(1.0f, (1.0f - HorizontalCorrectionAmount) * (1.0f - InterpolationAmount), BlendInAmount)
		};

		// Apply final offsets.

		TargetTransform.AddToTranslation(LocationOffset);
		TargetTransform.ConcatenateRotation(RotationOffset.Quaternion());
	}
	else
	{
		StopMantling();
	}
	
	TargetTransform.AddToTranslation(FVector(0.0f, 0.0f, GetRootCollisionHalfHeight(true)));

	SetActorTransform_GMC(TargetTransform, true);
}

void UAlsCharacterMovementComponent::StopMantling(const bool bStopMontage)
{
	MantlingState.MantlingTimer = 0.0f;
	
	SetLocomotionAction(AlsLocomotionActionTags::MantlingEnding);
	
	if (bStopMontage && MontageTracker.Montage != nullptr)
	{
		Montage_StopWithBlendOut(CharacterOwner->GetMesh()->GetAnimInstance(),
			FAlphaBlendArgs(Settings->Mantling.BlendOutDuration), MontageTracker.Montage);
	}
	
	SetMovementMode(EGMC_MovementMode::Grounded);
	
	OnMantlingEnded();
}

void UAlsCharacterMovementComponent::OnMantlingEnded_Implementation() {}

