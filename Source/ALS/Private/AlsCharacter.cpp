#include "AlsCharacter.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "TimerManager.h"
#include "Camera/AlsCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/PlayerController.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacter)

FName AAlsCharacter::MeshComponentName(TEXT("Mesh"));
FName AAlsCharacter::CharacterMovementComponentName(TEXT("CharacterMovement"));
FName AAlsCharacter::CapsuleComponentName(TEXT("CapsuleComponent"));

AAlsCharacter::AAlsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(CapsuleComponentName);
	if (CapsuleComponent) {
		CapsuleComponent->InitCapsuleSize(30.0f, 90.0f);
		CapsuleComponent->CanCharacterStepUpOn = ECB_No;
		CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
		CapsuleComponent->SetCanEverAffectNavigation(false);
		CapsuleComponent->bDynamicObstacle = true;
		CapsuleComponent->SetCollisionProfileName(FName("Pawn"));
		RootComponent = CapsuleComponent;
	}
	
	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(MeshComponentName);
	if (Mesh)
	{
		Mesh->AlwaysLoadOnClient = true;
		Mesh->AlwaysLoadOnServer = true;
		Mesh->bOwnerNoSee = false;
		Mesh->bCastDynamicShadow = true;
		Mesh->bAffectDynamicIndirectLighting = true;
		Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		Mesh->SetupAttachment(CapsuleComponent);
		static FName MeshCollisionProfileName(TEXT("CharacterMesh"));
		Mesh->SetCollisionProfileName(MeshCollisionProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	}
	
	GetMesh()->SetRelativeLocation_Direct({0.0f, 0.0f, -92.0f});
	GetMesh()->SetRelativeRotation_Direct({0.0f, -90.0f, 0.0f});

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	GetMesh()->bEnableUpdateRateOptimizations = false;

	Camera = CreateDefaultSubobject<UAlsCameraComponent>(FName{TEXTVIEW("Camera")});
	Camera->SetupAttachment(GetMesh());
	Camera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}


void AAlsCharacter::PostInitializeComponents()
{
	// Make sure the mesh and animation blueprint are ticking after the character so they can access the most up-to-date character state.

	GetMesh()->AddTickPrerequisiteActor(this);

	AlsCharacterMovement = FindComponentByClass<UAlsCharacterMovementComponent>();
	AlsCharacterMovement->SetSkeletalMeshReference(GetMesh());
	AlsCharacterMovement->SetUpdatedComponent(GetCapsuleComponent());
	
	AnimationInstance = Cast<UAlsAnimationInstance>(GetMesh()->GetAnimInstance());

	BaseTranslationOffset = Mesh->GetRelativeLocation();
	BaseRotationOffset = Mesh->GetRelativeRotation().Quaternion();

	Super::PostInitializeComponents();
}

void AAlsCharacter::BeginPlay()
{
	ALS_ENSURE(AnimationInstance.IsValid());

	ALS_ENSURE_MESSAGE(!bUseControllerRotationPitch && !bUseControllerRotationYaw && !bUseControllerRotationRoll,
	                   TEXT("These settings are not allowed and must be turned off!"));

	Super::BeginPlay();

}

void AAlsCharacter::FaceRotation(const FRotator Rotation, const float DeltaTime)
{
	// Left empty intentionally. We are ignoring rotation changes from external
	// sources because ALS itself has full control over character rotation.
}

void AAlsCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	if (!OnCalculateCamera(DeltaTime, ViewInfo))
	{
		if (Camera->IsActive())
		{
			Camera->GetViewInfo(ViewInfo);
			return;
		}

		Super::CalcCamera(DeltaTime, ViewInfo);
	}
}

bool AAlsCharacter::OnCalculateCamera_Implementation(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	return false;
}

//map GMC MovementMode to ALS LocomotionMode for use in the anim instance
//way too lazy to go through and change them all
const FGameplayTag AAlsCharacter::GetLocomotionMode() const
{
	switch (GetCharacterMovement()->GetMovementMode())
	{
	case EGMC_MovementMode::Grounded:
		return AlsLocomotionModeTags::Grounded;

	case EGMC_MovementMode::Airborne:
		return AlsLocomotionModeTags::InAir;

	default:
		return FGameplayTag::EmptyTag;

	}
}