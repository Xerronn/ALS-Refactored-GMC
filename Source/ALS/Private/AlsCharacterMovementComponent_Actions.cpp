// Fill out your copyright notice in the Description page of Project Settings.

#include "AlsCharacterMovementComponent.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/SkeletalMesh.h"
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

// bool UAlsCharacterMovementComponent::IsRagdollingAllowedToStart() const
// {
// 	return LocomotionAction != AlsLocomotionActionTags::Ragdolling;
// }
//
//
// void UAlsCharacterMovementComponent::StartRagdolling()
// {
// 	// if (GetLocalRole() <= ROLE_SimulatedProxy || !IsRagdollingAllowedToStart())
// 	// {
// 	// 	return;
// 	// }
// 	//
// 	// if (GetLocalRole() >= ROLE_Authority)
// 	// {
// 	// 	MulticastStartRagdolling();
// 	// }
// 	// else
// 	// {
// 	// 	GetCharacterMovement()->FlushServerMoves();
// 	//
// 	// 	ServerStartRagdolling();
// 	// }
// }
//
//
// void UAlsCharacterMovementComponent::StartRagdollingImplementation()
// {
// 	// if (!IsRagdollingAllowedToStart())
// 	// {
// 	// 	return;
// 	// }
// 	//
// 	// GetMesh()->bUpdateJointsFromAnimation = true; // Required for the flail animation to work properly.
// 	//
// 	// if (!GetMesh()->IsRunningParallelEvaluation() && GetMesh()->GetBoneSpaceTransforms().Num() > 0)
// 	// {
// 	// 	GetMesh()->UpdateRBJointMotors();
// 	// }
// 	//
// 	// // Stop any active montages.
// 	//
// 	// static constexpr auto BlendOutDuration{0.2f};
// 	//
// 	// GetMesh()->GetAnimInstance()->Montage_Stop(BlendOutDuration);
// 	//
// 	// // Disable movement corrections and reset network smoothing.
// 	//
// 	// GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
// 	// GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;
// 	//
// 	// // Detach the mesh so that character transformation changes will not affect it in any way.
// 	//
// 	// GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
// 	//
// 	// // Disable capsule collision and enable mesh physics simulation.
// 	//
// 	// GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
// 	//
// 	// GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
// 	// GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
// 	// GetMesh()->SetSimulatePhysics(true);
// 	//
// 	// const auto* PelvisBody{GetMesh()->GetBodyInstance(UAlsConstants::PelvisBoneName())};
// 	// FVector PelvisLocation;
// 	//
// 	// FPhysicsCommand::ExecuteRead(PelvisBody->ActorHandle, [this, &PelvisLocation](const FPhysicsActorHandle& ActorHandle)
// 	// {
// 	// 	PelvisLocation = FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true).GetLocation();
// 	// 	RagdollingState.Velocity = FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle);
// 	// });
// 	//
// 	// RagdollingState.PullForce = 0.0f;
// 	//
// 	// if (Settings->Ragdolling.bLimitInitialRagdollSpeed)
// 	// {
// 	// 	// Limit the ragdoll's speed for a few frames, because for some unclear reason,
// 	// 	// it can get a much higher initial speed than the character's last speed.
// 	//
// 	// 	// TODO Find a better solution or wait for a fix in future engine versions.
// 	//
// 	// 	static constexpr auto MinSpeedLimit{200.0f};
// 	//
// 	// 	RagdollingState.SpeedLimitFrameTimeRemaining = 8;
// 	// 	RagdollingState.SpeedLimit = FMath::Max(MinSpeedLimit, UE_REAL_TO_FLOAT(LocomotionState.Velocity.Size()));
// 	//
// 	// 	ConstraintRagdollSpeed();
// 	// }
// 	//
// 	// if (GetLocalRole() >= ROLE_Authority)
// 	// {
// 	// 	SetRagdollTargetLocation(FVector::ZeroVector);
// 	// }
// 	//
// 	// if (IsLocallyControlled() || (GetLocalRole() >= ROLE_Authority && !IsValid(GetController())))
// 	// {
// 	// 	SetRagdollTargetLocation(PelvisLocation);
// 	// }
// 	//
// 	// // Clear the character movement mode and set the locomotion action to ragdolling.
// 	//
// 	// GetCharacterMovement()->SetMovementMode(MOVE_None);
// 	// AlsCharacterMovement->SetMovementModeLocked(true);
// 	//
// 	// SetLocomotionAction(AlsLocomotionActionTags::Ragdolling);
// 	//
// 	// OnRagdollingStarted();
// }
//
// void UAlsCharacterMovementComponent::SetRagdollTargetLocation(const FVector& NewTargetLocation)
// {
// 	RagdollTargetLocation = NewTargetLocation;
// }
//
// void UAlsCharacterMovementComponent::RefreshRagdolling(const float DeltaTime)
// {
// 	// if (LocomotionAction != AlsLocomotionActionTags::Ragdolling)
// 	// {
// 	// 	return;
// 	// }
// 	//
// 	// // Since we are dealing with physics here, we should not use functions such as USkinnedMeshComponent::GetSocketTransform() as
// 	// // they may return an incorrect result in situations like when the animation blueprint is not ticking or when URO is enabled.
// 	//
// 	// const auto* PelvisBody{GetMesh()->GetBodyInstance(UAlsConstants::PelvisBoneName())};
// 	// FVector PelvisLocation;
// 	//
// 	// FPhysicsCommand::ExecuteRead(PelvisBody->ActorHandle, [this, &PelvisLocation](const FPhysicsActorHandle& ActorHandle)
// 	// {
// 	// 	PelvisLocation = FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true).GetLocation();
// 	// 	RagdollingState.Velocity = FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle);
// 	// });
// 	//
// 	// const auto bLocallyControlled{IsLocallyControlled() || (GetLocalRole() >= ROLE_Authority && !IsValid(GetController()))};
// 	//
// 	// if (bLocallyControlled)
// 	// {
// 	// 	SetRagdollTargetLocation(PelvisLocation);
// 	// }
// 	//
// 	// // Prevent the capsule from going through the ground when the ragdoll is lying on the ground.
// 	//
// 	// // While we could get rid of the line trace here and just use RagdollTargetLocation
// 	// // as the character's location, we don't do that because the camera depends on the
// 	// // capsule's bottom location, so its removal will cause the camera to behave erratically.
// 	//
// 	// bool bGrounded;
// 	// SetActorLocation(RagdollTraceGround(bGrounded), false, nullptr, ETeleportType::TeleportPhysics);
// 	//
// 	// // Zero target location means that it hasn't been replicated yet, so we can't apply the logic below.
// 	//
// 	// if (!bLocallyControlled && !RagdollTargetLocation.IsZero())
// 	// {
// 	// 	// Apply ragdoll location corrections.
// 	//
// 	// 	static constexpr auto PullForce{750.0f};
// 	// 	static constexpr auto InterpolationSpeed{0.6f};
// 	//
// 	// 	RagdollingState.PullForce = FMath::FInterpTo(RagdollingState.PullForce, PullForce, DeltaTime, InterpolationSpeed);
// 	//
// 	// 	const auto HorizontalSpeedSquared{RagdollingState.Velocity.SizeSquared2D()};
// 	//
// 	// 	const auto PullForceBoneName{
// 	// 		HorizontalSpeedSquared > FMath::Square(300.0f) ? UAlsConstants::Spine03BoneName() : UAlsConstants::PelvisBoneName()
// 	// 	};
// 	//
// 	// 	auto* PullForceBody{GetMesh()->GetBodyInstance(PullForceBoneName)};
// 	//
// 	// 	FPhysicsCommand::ExecuteWrite(PullForceBody->ActorHandle, [this](const FPhysicsActorHandle& ActorHandle)
// 	// 	{
// 	// 		if (!FPhysicsInterface::IsRigidBody(ActorHandle))
// 	// 		{
// 	// 			return;
// 	// 		}
// 	//
// 	// 		const auto PullForceVector{
// 	// 			RagdollTargetLocation - FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true).GetLocation()
// 	// 		};
// 	//
// 	// 		static constexpr auto MinPullForceDistance{5.0f};
// 	// 		static constexpr auto MaxPullForceDistance{50.0f};
// 	//
// 	// 		if (PullForceVector.SizeSquared() > FMath::Square(MinPullForceDistance))
// 	// 		{
// 	// 			FPhysicsInterface::AddForce_AssumesLocked(
// 	// 				ActorHandle, PullForceVector.GetClampedToMaxSize(MaxPullForceDistance) * RagdollingState.PullForce, true, true);
// 	// 		}
// 	// 	});
// 	// }
// 	//
// 	// // Use the speed to scale ragdoll joint strength for physical animation.
// 	//
// 	// static constexpr auto ReferenceSpeed{1000.0f};
// 	// static constexpr auto Stiffness{25000.0f};
// 	//
// 	// const auto SpeedAmount{UAlsMath::Clamp01(UE_REAL_TO_FLOAT(RagdollingState.Velocity.Size() / ReferenceSpeed))};
// 	//
// 	// GetMesh()->SetAllMotorsAngularDriveParams(SpeedAmount * Stiffness, 0.0f, 0.0f);
// 	//
// 	// // Limit the speed of ragdoll bodies.
// 	//
// 	// if (RagdollingState.SpeedLimitFrameTimeRemaining > 0)
// 	// {
// 	// 	RagdollingState.SpeedLimitFrameTimeRemaining -= 1;
// 	//
// 	// 	ConstraintRagdollSpeed();
// 	// }
// }
//
// FVector UAlsCharacterMovementComponent::RagdollTraceGround(bool& bGrounded) const
// {
// 	// auto RagdollLocation{!RagdollTargetLocation.IsZero() ? FVector{RagdollTargetLocation} : GetActorLocation()};
// 	//
// 	// // We use a sphere sweep instead of a simple line trace to keep capsule
// 	// // movement consistent between ragdolling and regular character movement.
// 	//
// 	// const auto CapsuleRadius{GetCapsuleComponent()->GetScaledCapsuleRadius()};
// 	// const auto CapsuleHalfHeight{GetCapsuleComponent()->GetScaledCapsuleHalfHeight()};
// 	//
// 	// const FVector TraceStart{RagdollLocation.X, RagdollLocation.Y, RagdollLocation.Z + 2.0f * CapsuleRadius};
// 	// const FVector TraceEnd{RagdollLocation.X, RagdollLocation.Y, RagdollLocation.Z - CapsuleHalfHeight + CapsuleRadius};
// 	//
// 	// const auto CollisionChannel{GetCharacterMovement()->UpdatedComponent->GetCollisionObjectType()};
// 	//
// 	// FCollisionQueryParams QueryParameters{__FUNCTION__, false, this};
// 	// FCollisionResponseParams ResponseParameters;
// 	// GetCharacterMovement()->InitCollisionParams(QueryParameters, ResponseParameters);
// 	//
// 	// FHitResult Hit;
// 	// bGrounded = GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity,
// 	//                                              CollisionChannel, FCollisionShape::MakeSphere(CapsuleRadius),
// 	//                                              QueryParameters, ResponseParameters);
// 	//
// 	// // #if ENABLE_DRAW_DEBUG
// 	// // 	UAlsDebugUtility::DrawSweepSingleSphere(GetWorld(), TraceStart, TraceEnd, CapsuleRadius,
// 	// // 	                                        bGrounded, Hit, {0.0f, 0.25f, 1.0f},
// 	// // 	                                        {0.0f, 0.75f, 1.0f}, 0.0f);
// 	// // #endif
// 	//
// 	// return FVector{
// 	// 	RagdollLocation.X, RagdollLocation.Y,
// 	// 	bGrounded
// 	// 		? Hit.Location.Z + CapsuleHalfHeight - CapsuleRadius + UCharacterMovementComponent::MIN_FLOOR_DIST
// 	// 		: RagdollLocation.Z
// 	// };
// 	return FVector::ZeroVector;
// }
//
// void UAlsCharacterMovementComponent::ConstraintRagdollSpeed() const
// {
// 	// GetMesh()->ForEachBodyBelow(NAME_None, true, false, [this](FBodyInstance* Body)
// 	// {
// 	// 	FPhysicsCommand::ExecuteWrite(Body->ActorHandle, [this](const FPhysicsActorHandle& ActorHandle)
// 	// 	{
// 	// 		if (!FPhysicsInterface::IsRigidBody(ActorHandle))
// 	// 		{
// 	// 			return;
// 	// 		}
// 	//
// 	// 		auto Velocity{FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle)};
// 	// 		if (Velocity.SizeSquared() <= FMath::Square(RagdollingState.SpeedLimit))
// 	// 		{
// 	// 			return;
// 	// 		}
// 	//
// 	// 		Velocity.Normalize();
// 	// 		Velocity *= RagdollingState.SpeedLimit;
// 	//
// 	// 		FPhysicsInterface::SetLinearVelocity_AssumesLocked(ActorHandle, Velocity);
// 	// 	});
// 	// });
// }
//
// bool UAlsCharacterMovementComponent::IsRagdollingAllowedToStop() const
// {
// 	return LocomotionAction == AlsLocomotionActionTags::Ragdolling;
// }
//
// bool UAlsCharacterMovementComponent::StopRagdolling()
// {
// 	// if (GetLocalRole() <= ROLE_SimulatedProxy || !IsRagdollingAllowedToStop())
// 	// {
// 	// 	return false;
// 	// }
// 	//
// 	// if (GetLocalRole() >= ROLE_Authority)
// 	// {
// 	// 	MulticastStopRagdolling();
// 	// }
// 	// else
// 	// {
// 	// 	ServerStopRagdolling();
// 	// }
// 	//
// 	return true;
// }
//
// void UAlsCharacterMovementComponent::StopRagdollingImplementation()
// {
// 	// if (!IsRagdollingAllowedToStop())
// 	// {
// 	// 	return;
// 	// }
// 	//
// 	// auto& FinalRagdollPose{AnimationInstance->SnapshotFinalRagdollPose()};
// 	//
// 	// const auto PelvisTransform{GetMesh()->GetSocketTransform(UAlsConstants::PelvisBoneName())};
// 	// const auto PelvisRotation{PelvisTransform.Rotator()};
// 	//
// 	// // Disable mesh physics simulation and enable capsule collision.
// 	//
// 	// GetMesh()->bUpdateJointsFromAnimation = false;
// 	//
// 	// GetMesh()->SetSimulatePhysics(false);
// 	// GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
// 	// GetMesh()->SetCollisionObjectType(ECC_Pawn);
// 	//
// 	// GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
// 	//
// 	// GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
// 	// GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
// 	//
// 	// bool bGrounded;
// 	// const auto NewActorLocation{RagdollTraceGround(bGrounded)};
// 	//
// 	// // Determine whether the ragdoll is facing upward or downward and set the actor rotation accordingly.
// 	//
// 	// const auto bRagdollFacingUpward{FRotator::NormalizeAxis(PelvisRotation.Roll) <= 0.0f};
// 	//
// 	// auto NewActorRotation{GetActorRotation()};
// 	// NewActorRotation.Yaw = bRagdollFacingUpward ? PelvisRotation.Yaw - 180.0f : PelvisRotation.Yaw;
// 	//
// 	// SetActorLocationAndRotation(NewActorLocation, NewActorRotation, false, nullptr, ETeleportType::TeleportPhysics);
// 	//
// 	// // Attach the mesh back and restore its default relative location.
// 	//
// 	// const auto& ActorTransform{GetActorTransform()};
// 	//
// 	// GetMesh()->SetWorldLocationAndRotationNoPhysics(ActorTransform.TransformPositionNoScale(GetBaseTranslationOffset()),
// 	//                                                 ActorTransform.TransformRotation(GetBaseRotationOffset()).Rotator());
// 	//
// 	// GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
// 	//
// 	// if (GetMesh()->ShouldUseUpdateRateOptimizations())
// 	// {
// 	// 	// Disable URO for one frame to force the animation blueprint to update and get rid of the incorrect mesh pose.
// 	//
// 	// 	GetMesh()->bEnableUpdateRateOptimizations = false;
// 	//
// 	// 	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]
// 	// 	{
// 	// 		ALS_ENSURE(!GetMesh()->bEnableUpdateRateOptimizations);
// 	// 		GetMesh()->bEnableUpdateRateOptimizations = true;
// 	// 	}));
// 	// }
// 	//
// 	// // Restore the pelvis transform to the state it was in before we changed
// 	// // the character and mesh transforms to keep its world transform unchanged.
// 	//
// 	// const auto& ReferenceSkeleton{GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton()};
// 	//
// 	// const auto PelvisBoneIndex{ReferenceSkeleton.FindBoneIndex(UAlsConstants::PelvisBoneName())};
// 	// if (ALS_ENSURE(PelvisBoneIndex >= 0))
// 	// {
// 	// 	// We expect the pelvis bone to be the root bone or attached to it, so we can safely use the mesh transform here.
// 	// 	FinalRagdollPose.LocalTransforms[PelvisBoneIndex] = PelvisTransform.GetRelativeTransform(GetMesh()->GetComponentTransform());
// 	// }
// 	//
// 	// // If the ragdoll is on the ground, set the movement mode to walking and play a get up montage. If not, set
// 	// // the movement mode to falling and update the character movement velocity to match the last ragdoll velocity.
// 	//
// 	// AlsCharacterMovement->SetMovementModeLocked(false);
// 	//
// 	// if (bGrounded)
// 	// {
// 	// 	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
// 	// }
// 	// else
// 	// {
// 	// 	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
// 	// 	GetCharacterMovement()->Velocity = RagdollingState.Velocity;
// 	// }
// 	//
// 	// SetLocomotionAction(FGameplayTag::EmptyTag);
// 	//
// 	// OnRagdollingEnded();
// 	//
// 	// if (bGrounded && GetMesh()->GetAnimInstance()->Montage_Play(SelectGetUpMontage(bRagdollFacingUpward)) > 0.0f)
// 	// {
// 	// 	AlsCharacterMovement->SetInputBlocked(true);
// 	//
// 	// 	SetLocomotionAction(AlsLocomotionActionTags::GettingUp);
// 	// }
// }
//
// UAnimMontage* UAlsCharacterMovementComponent::SelectGetUpMontage_Implementation(const bool bRagdollFacingUpward)
// {
// 	return bRagdollFacingUpward ? Settings->Ragdolling.GetUpBackMontage : Settings->Ragdolling.GetUpFrontMontage;
// }
