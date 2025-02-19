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
	return bCanJump && Stance == AlsStanceTags::Standing && IsMovingOnGround();

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
	if (!SkeletalMesh)
	{
		return;
	}
	const FVector CurrentRelativeLocation = SkeletalMesh->GetRelativeLocation();
	SkeletalMesh->SetRelativeLocation({CurrentRelativeLocation.X, CurrentRelativeLocation.Y, -GetRootCollisionHalfHeight(true)});
}

void UAlsCharacterMovementComponent::MaintainMeshOffsetSimulated()
{
	if (!SkeletalMesh)
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

	TObjectPtr CharacterMesh = CharacterOwner->GetMesh();
	
	CharacterMesh->bUpdateJointsFromAnimation = true; // Required for the flail animation to work properly.
	
	if (!CharacterMesh->IsRunningParallelEvaluation() && CharacterMesh->GetBoneSpaceTransforms().Num() > 0)
	{
		CharacterMesh->UpdateRBJointMotors();
	}
	
	// Stop any active montages.
	
	static constexpr auto BlendOutDuration{0.2f};
	
	CharacterMesh->GetAnimInstance()->Montage_Stop(BlendOutDuration);

	//detach mesh so that the capsule updates do not affect the mesh
	CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	
	// Disable capsule collision and enable mesh physics simulation.
	
	CharacterOwner->GetCapsuleComponent()->SetCollisionProfileName(FName("Spectator"), true);
	
	CharacterMesh->SetCollisionObjectType(ECC_PhysicsBody);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterMesh->SetSimulatePhysics(true);

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
	
	// Since we are dealing with physics here, we should not use functions such as USkinnedMeshComponent::GetSocketTransform() as
	// they may return an incorrect result in situations like when the animation blueprint is not ticking or when URO is enabled.
	
	const auto* PelvisBody{CharacterMesh->GetBodyInstance(UAlsConstants::PelvisBoneName())};
	FVector PelvisLocation;
	FRotator PelvisRotation;
	
	FPhysicsCommand::ExecuteRead(PelvisBody->ActorHandle, [this, &PelvisLocation, &PelvisRotation](const FPhysicsActorHandle& ActorHandle)
	{
		FTransform PevlisTransform = FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true);
		PelvisLocation = PevlisTransform.GetLocation();
		PelvisRotation = PevlisTransform.GetRotation().Rotator();
		RagdollingState.Velocity = FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle);
	});

	//set the ragdoll target location from the authority
	if (GetOwnerRole() >= ROLE_Authority)
	{
		RagdollingState.TargetLocation = PelvisLocation;
		RagdollingState.TargetRotation = PelvisRotation;
	}

	if (!RagdollingState.TargetLocation.IsZero())
	{
		bool bGrounded;
		SetActorLocation_GMC(RagdollTraceGround(bGrounded), true);
	}
	
	if (GetOwnerRole() < ROLE_Authority && !RagdollingState.TargetLocation.IsZero())
	{
		// Apply ragdoll location corrections.
	
		static constexpr auto PullForce{750.0f};
		static constexpr auto InterpolationSpeed{0.6f};
	
		RagdollingState.PullForce = FMath::FInterpTo(RagdollingState.PullForce, PullForce, DeltaTime, InterpolationSpeed);
	
		const auto HorizontalSpeedSquared{RagdollingState.Velocity.SizeSquared2D()};
	
		const auto PullForceBoneName{
			HorizontalSpeedSquared > FMath::Square(300.0f) ? UAlsConstants::Spine03BoneName() : UAlsConstants::PelvisBoneName()
		};
	
		auto* PullForceBody{CharacterMesh->GetBodyInstance(PullForceBoneName)};
	
		FPhysicsCommand::ExecuteWrite(PullForceBody->ActorHandle, [this](const FPhysicsActorHandle& ActorHandle)
		{
			if (!FPhysicsInterface::IsRigidBody(ActorHandle))
			{
				return;
			}
		
			const auto PullForceVector{
				RagdollingState.TargetLocation - FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true).GetLocation()
			};
		
			static constexpr auto MinPullForceDistance{5.0f};
			static constexpr auto MaxPullForceDistance{50.0f};
		
			if (PullForceVector.SizeSquared() > FMath::Square(MinPullForceDistance))
			{
				FPhysicsInterface::AddForce_AssumesLocked(
					ActorHandle, PullForceVector.GetClampedToMaxSize(MaxPullForceDistance) * RagdollingState.PullForce, true, true);
			}
		});
	}
	
	// Use the speed to scale ragdoll joint strength for physical animation.
	
	static constexpr auto ReferenceSpeed{1000.0f};
	static constexpr auto Stiffness{25000.0f};
	
	const auto SpeedAmount{UAlsMath::Clamp01(UE_REAL_TO_FLOAT(RagdollingState.Velocity.Size() / ReferenceSpeed))};
	
	CharacterMesh->SetAllMotorsAngularDriveParams(SpeedAmount * Stiffness, 0.0f, 0.0f);
	
	// Limit the speed of ragdoll bodies.
	
	// if (RagdollingState.SpeedLimitFrameTimeRemaining > 0)
	// {
	// 	RagdollingState.SpeedLimitFrameTimeRemaining -= 1;
	//
	// 	ConstraintRagdollSpeed();
	// }
}

FVector UAlsCharacterMovementComponent::RagdollTraceGround(bool& bGrounded) const
{
	auto RagdollLocation{!RagdollingState.TargetLocation.IsZero() ? RagdollingState.TargetLocation : GetActorLocation()};
	
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
	
	#if ENABLE_DRAW_DEBUG
		UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), TraceStart, TraceEnd, CapsuleRadius,
		                                        bGrounded, Hit, {0.0f, 0.25f, 1.0f},
		                                        {0.0f, 0.75f, 1.0f}, 0.0f);
	#endif
	
	return FVector{
		RagdollLocation.X, RagdollLocation.Y,
		bGrounded
			? Hit.Location.Z + CapsuleHalfHeight - CapsuleRadius + 1.9f
			: RagdollLocation.Z
	};
}

void UAlsCharacterMovementComponent::ConstraintRagdollSpeed() const
{
	CharacterOwner->GetMesh()->ForEachBodyBelow(NAME_None, true, false, [this](FBodyInstance* Body)
	{
		FPhysicsCommand::ExecuteWrite(Body->ActorHandle, [this](const FPhysicsActorHandle& ActorHandle)
		{
			if (!FPhysicsInterface::IsRigidBody(ActorHandle))
			{
				return;
			}
	
			auto Velocity{FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle)};
			if (Velocity.SizeSquared() <= FMath::Square(RagdollingState.SpeedLimit))
			{
				return;
			}
	
			Velocity.Normalize();
			Velocity *= RagdollingState.SpeedLimit;
	
			FPhysicsInterface::SetLinearVelocity_AssumesLocked(ActorHandle, Velocity);
		});
	});
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
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionObjectType(ECC_Pawn);

	TObjectPtr CharacterCapsule = CharacterOwner->GetCapsuleComponent();
	CharacterCapsule->SetCollisionProfileName(FName("Pawn"), true);
	
	bool bGrounded;
	const auto NewActorLocation{RagdollTraceGround(bGrounded)};
	
	// Determine whether the ragdoll is facing upward or downward and set the actor rotation accordingly.
	
	const auto bRagdollFacingUpward{FRotator::NormalizeAxis(RagdollingState.TargetRotation.Roll) <= 0.0f};
	
	auto NewActorRotation{GetActorRotation_GMC()};
	NewActorRotation.Yaw = bRagdollFacingUpward ? RagdollingState.TargetRotation.Yaw - 180.0f : RagdollingState.TargetRotation.Yaw;
	
	SetActorLocationAndRotation_GMC(NewActorLocation, NewActorRotation, false);
	
	// If the ragdoll is on the ground, set the movement mode to walking and play a get up montage. If not, set
	// the movement mode to falling and update the character movement velocity to match the last ragdoll velocity.

	const auto& ActorTransform{GetActorTransform()};

	CharacterMesh->SetWorldLocationAndRotationNoPhysics(ActorTransform.TransformPositionNoScale(CharacterOwner->GetBaseTranslationOffset()),
													ActorTransform.TransformRotation(CharacterOwner->GetBaseRotationOffset()).Rotator());

	CharacterMesh->AttachToComponent(CharacterCapsule, FAttachmentTransformRules::KeepWorldTransform);

	// Restore the pelvis transform to the state it was in before we changed
	// the character and mesh transforms to keep its world transform unchanged.

	const auto& ReferenceSkeleton{CharacterMesh->GetSkinnedAsset()->GetRefSkeleton()};

	const auto PelvisBoneIndex{ReferenceSkeleton.FindBoneIndex(UAlsConstants::PelvisBoneName())};
	if (ALS_ENSURE(PelvisBoneIndex >= 0))
	{
		// We expect the pelvis bone to be the root bone or attached to it, so we can safely use the mesh transform here.
		FinalRagdollPose.LocalTransforms[PelvisBoneIndex] = PelvisTransform.GetRelativeTransform(CharacterMesh->GetComponentTransform());
	}

	CharacterOwner->GetCapsuleComponent()->SetCollisionProfileName(FName("Pawn"), false);
	
	SetLocomotionAction(FGameplayTag::EmptyTag);
	RagdollingState.TargetLocation = FVector::ZeroVector;
	
	OnRagdollingEnded();
	
	if (bGrounded && CharacterMesh->GetAnimInstance()->Montage_Play(SelectGetUpMontage(bRagdollFacingUpward)) > 0.0f)
	{
		// AlsCharacterMovement->SetInputBlocked(true);
	
		SetLocomotionAction(AlsLocomotionActionTags::GettingUp);
	}
	return true;
}

void UAlsCharacterMovementComponent::OnRagdollingEnded_Implementation() {}

UAnimMontage* UAlsCharacterMovementComponent::SelectGetUpMontage_Implementation(const bool bRagdollFacingUpward)
{
	return bRagdollFacingUpward ? Settings->Ragdolling.GetUpBackMontage : Settings->Ragdolling.GetUpFrontMontage;
}
