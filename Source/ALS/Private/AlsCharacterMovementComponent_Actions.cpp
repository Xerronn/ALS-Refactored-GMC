// Fill out your copyright notice in the Description page of Project Settings.

#include "AlsCharacterMovementComponent.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "RootMotionSources/AlsRootMotionSource_Mantling.h"
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
	if (!SkeletalMesh || LocomotionAction.IsValid())
	{
		return;
	}
	const FVector CurrentRelativeLocation = SkeletalMesh->GetRelativeLocation();
	SkeletalMesh->SetRelativeLocation({CurrentRelativeLocation.X, CurrentRelativeLocation.Y, -GetRootCollisionHalfHeight(true)});
}

void UAlsCharacterMovementComponent::MaintainMeshOffsetSimulated()
{
	if (!SkeletalMesh || LocomotionAction.IsValid())
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

void UAlsCharacterMovementComponent::StartRagdolling()
{
	if (!IsRagdollingAllowedToStart())
	{
		return;
	}

	RagdollingState.TargetLocation = FVector::ZeroVector;
	RagdollingState.TargetRotation = FRotator::ZeroRotator;

	TObjectPtr CharacterMesh = CharacterOwner->GetMesh();
	
	CharacterMesh->bUpdateJointsFromAnimation = true; // Required for the flail animation to work properly.
	
	if (!CharacterMesh->IsRunningParallelEvaluation() && CharacterMesh->GetBoneSpaceTransforms().Num() > 0)
	{
		CharacterMesh->UpdateRBJointMotors();
	}
	
	// Stop any active montages.
	static constexpr auto BlendOutDuration{0.2f};
	CharacterMesh->GetAnimInstance()->Montage_Stop(BlendOutDuration);
	if (MontageTracker.HasActiveRootMotionMontage())
	{
		MontageTracker.ClearActiveMontage();
	}
	RootMotionParams.Clear();
	bHasRootMotion = false;

	//detach mesh so that the capsule updates do not affect the mesh
	SetComponentToSmooth(nullptr);
	CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	
	// Disable capsule collision and enable mesh physics simulation.
	CharacterOwner->GetCapsuleComponent()->SetCollisionProfileName(FName("Spectator"), false);
	CharacterMesh->SetCollisionObjectType(ECC_PhysicsBody);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	if (GetOwnerRole() >= ROLE_Authority)
	{
		CharacterMesh->SetSimulatePhysics(true);
	} else
	{
		CharacterMesh->SetAllBodiesBelowSimulatePhysics(UAlsConstants::PelvisBoneName(),true, false);
	}
	
	CharacterMesh->SetAllBodiesBelowLinearVelocity(UAlsConstants::PelvisBoneName(), GetLinearVelocity_GMC(), true);
	
	SetLocomotionAction(AlsLocomotionActionTags::Ragdolling);
	
	OnRagdollingStarted();
}

void UAlsCharacterMovementComponent::RefreshRagdolling(const float DeltaTime)
{
	if (LocomotionAction != AlsLocomotionActionTags::Ragdolling)
	{
		return;
	}

	TObjectPtr CharacterMesh = CharacterOwner->GetMesh();
	
	//set the ragdoll target location from the authority
	if (GetOwnerRole() >= ROLE_Authority)
	{
		SkeletalMesh->GetSocketWorldLocationAndRotation(
			UAlsConstants::PelvisBoneName(),
			RagdollingState.TargetLocation,
			RagdollingState.TargetRotation
			);
	}

	if (!RagdollingState.TargetLocation.IsZero())
	{
		bool bGrounded;
		SetActorLocation_GMC(RagdollTraceGround(bGrounded), true);
	}
	
	// Use the speed to scale ragdoll joint strength for physical animation.
	
	static constexpr auto ReferenceSpeed{1000.0f};
	static constexpr auto Stiffness{25000.0f};
	
	const auto SpeedAmount{UAlsMath::Clamp01(UE_REAL_TO_FLOAT(GetLinearVelocity_GMC().Size() / ReferenceSpeed))};
	
	CharacterMesh->SetAllMotorsAngularDriveParams(SpeedAmount * Stiffness, 0.0f, 0.0f);
	
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

bool UAlsCharacterMovementComponent::StopRagdolling()
{
	if (!IsRagdollingAllowedToStop())
	{
		return false;
	}
	TObjectPtr CharacterMesh = CharacterOwner->GetMesh();
	auto& FinalRagdollPose{CharacterOwner->GetAnimInstance()->SnapshotFinalRagdollPose()};
	
	const auto PelvisTransform{CharacterMesh->GetSocketTransform(UAlsConstants::PelvisBoneName())};
	
	// Disable mesh physics simulation and enable capsule collision.
	CharacterMesh->bUpdateJointsFromAnimation = false;
	
	CharacterMesh->SetSimulatePhysics(false);
	CharacterMesh->SetAllBodiesSimulatePhysics(false);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionObjectType(ECC_Pawn);

	TObjectPtr CharacterCapsule = CharacterOwner->GetCapsuleComponent();
	CharacterCapsule->SetCollisionProfileName(FName("Pawn"), false);
	
	bool bGrounded;
	const auto NewActorLocation{RagdollTraceGround(bGrounded)};
	
	// Determine whether the ragdoll is facing upward or downward and set the actor rotation accordingly.
	
	const auto bRagdollFacingUpward{FRotator::NormalizeAxis(RagdollingState.TargetRotation.Roll) <= 0.0f};
	
	auto NewActorRotation{GetActorRotation_GMC()};
	NewActorRotation.Yaw = bRagdollFacingUpward ? RagdollingState.TargetRotation.Yaw - 180.0f : RagdollingState.TargetRotation.Yaw;
	
	SetActorLocationAndRotation_GMC(NewActorLocation, NewActorRotation, false);
	
	const auto& ActorTransform{GetActorTransform()};
	
	CharacterMesh->SetWorldLocationAndRotationNoPhysics(ActorTransform.TransformPositionNoScale(CharacterOwner->GetBaseTranslationOffset()),
													ActorTransform.TransformRotation(CharacterOwner->GetBaseRotationOffset()).Rotator());
	
	CharacterMesh->AttachToComponent(CharacterCapsule, FAttachmentTransformRules::KeepWorldTransform);
	
	SetComponentToSmooth(CharacterMesh);
	
	// Restore the pelvis transform to the state it was in before we changed
	// the character and mesh transforms to keep its world transform unchanged.
	
	const auto& ReferenceSkeleton{CharacterMesh->GetSkinnedAsset()->GetRefSkeleton()};
	
	const auto PelvisBoneIndex{ReferenceSkeleton.FindBoneIndex(UAlsConstants::PelvisBoneName())};
	if (ALS_ENSURE(PelvisBoneIndex >= 0))
	{
		// We expect the pelvis bone to be the root bone or attached to it, so we can safely use the mesh transform here.
		FinalRagdollPose.LocalTransforms[PelvisBoneIndex] = PelvisTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());
	}
	
	
	SetLocomotionAction(FGameplayTag::EmptyTag);
	
	OnRagdollingEnded();
	
	if (bGrounded && PlayMontage_Blocking(CharacterOwner->GetMesh(), MontageTracker, SelectGetUpMontage(bRagdollFacingUpward), 0.0f, 1.0f) > 0.0f)
	{
		SetLocomotionAction(AlsLocomotionActionTags::GettingUp);
	}
	
	return true;
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
}

UAnimMontage* UAlsCharacterMovementComponent::SelectRollMontage_Implementation()
{
	return Settings->Rolling.Montage;
}
