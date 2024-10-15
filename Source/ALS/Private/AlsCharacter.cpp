#include "AlsCharacter.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Settings/AlsCharacterSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsRotation.h"
#include "Utility/AlsUtility.h"
#include "Utility/AlsVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacter)

namespace AlsCharacterConstants
{
	constexpr auto TeleportDistanceThresholdSquared{FMath::Square(50.0f)};
	constexpr auto MinAimingYawAngleLimit{70.0f};
}

AAlsCharacter::AAlsCharacter(const FObjectInitializer& ObjectInitializer) : Super{
	ObjectInitializer.SetDefaultSubobjectClass<UAlsCharacterMovementComponent>(CharacterMovementComponentName)
}
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(30.0f, 90.0f);
	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;
	RootComponent = CapsuleComponent;

	AlsCharacterMovement = CreateDefaultSubobject<UAlsCharacterMovementComponent>(CharacterMovementComponentName);

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
	}
	
	GetMesh()->SetRelativeLocation_Direct({0.0f, 0.0f, -92.0f});
	GetMesh()->SetRelativeRotation_Direct({0.0f, -90.0f, 0.0f});

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	GetMesh()->bEnableUpdateRateOptimizations = false;

	// This will prevent the editor from combining component details with actor details.
	// Component details can still be accessed from the actor's component hierarchy.

#if WITH_EDITOR
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("Mesh")})->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("CapsuleComponent")})->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("CharacterMovement")})->SetPropertyFlags(CPF_DisableEditOnInstance);
#endif
}

#if WITH_EDITOR
bool AAlsCharacter::CanEditChange(const FProperty* Property) const
{
	return Super::CanEditChange(Property) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationPitch) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationYaw) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationRoll);
}
#endif


void AAlsCharacter::PostInitializeComponents()
{
	// Make sure the mesh and animation blueprint are ticking after the character so they can access the most up-to-date character state.

	GetMesh()->AddTickPrerequisiteActor(this);
	
	AnimationInstance = Cast<UAlsAnimationInstance>(GetMesh()->GetAnimInstance());

	Super::PostInitializeComponents();
}

void AAlsCharacter::BeginPlay()
{
	ALS_ENSURE(AnimationInstance.IsValid());

	ALS_ENSURE_MESSAGE(!bUseControllerRotationPitch && !bUseControllerRotationYaw && !bUseControllerRotationRoll,
	                   TEXT("These settings are not allowed and must be turned off!"));

	Super::BeginPlay();

	AlsCharacterMovement->SetSkeletalMeshReference(GetMesh());
	AlsCharacterMovement->SetUpdatedComponent(GetCapsuleComponent());

	RefreshMeshProperties();
}

void AAlsCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	if (!OnCalculateCamera(DeltaTime, ViewInfo))
	{
		Super::CalcCamera(DeltaTime, ViewInfo);
	}
}

void AAlsCharacter::Tick(const float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("AAlsCharacter::Tick"), STAT_AAlsCharacter_Tick, STATGROUP_Als)
	TRACE_CPUPROFILER_EVENT_SCOPE(AAlsCharacter::Tick);

	if (!AnimationInstance.IsValid())
	{
		Super::Tick(DeltaTime);
		return;
	}

	RefreshMeshProperties();

	Super::Tick(DeltaTime);
}

void AAlsCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	RefreshMeshProperties();
}

bool AAlsCharacter::OnCalculateCamera_Implementation(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	return false;
}

void AAlsCharacter::RefreshMeshProperties() const
{
	const auto bStandalone{IsNetMode(NM_Standalone)};

	const auto bAuthority{GetLocalRole() >= ROLE_Authority};
	const auto bRemoteAutonomousProxy{GetRemoteRole() == ROLE_AutonomousProxy};

	// Make sure that the pose is always ticked on the server when the character is controlled
	// by a remote client, otherwise some problems may arise (such as jitter when rolling).

	const auto DefaultTickOption{GetClass()->GetDefaultObject<ThisClass>()->GetMesh()->VisibilityBasedAnimTickOption};

	const auto TargetTickOption{
		!bStandalone && bAuthority && bRemoteAutonomousProxy
			? EVisibilityBasedAnimTickOption::AlwaysTickPose
			: EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered
	};

	// Keep the default tick option, at least if the target tick option is not required by the plugin to work properly.

	GetMesh()->VisibilityBasedAnimTickOption = FMath::Min(TargetTickOption, DefaultTickOption);

	const auto bMeshIsTicking{
		GetMesh()->bRecentlyRendered || GetMesh()->VisibilityBasedAnimTickOption <= EVisibilityBasedAnimTickOption::AlwaysTickPose
	};
	
	if (!bMeshIsTicking)
	{
		AnimationInstance->MarkPendingUpdate();
	}
}

FRotator AAlsCharacter::GetViewRotation() const
{
	return AlsCharacterMovement->ViewState.Rotation;
}

//map GMC MovementMode to ALS LocomotionMode for use in the anim instance
//way too lazy to go through and change them all
const FGameplayTag& AAlsCharacter::GetLocomotionMode() const
{
	switch (AlsCharacterMovement->GetMovementMode())
	{
	case EGMC_MovementMode::Grounded:
		return AlsLocomotionModeTags::Grounded;

	case EGMC_MovementMode::Airborne:
		return AlsLocomotionModeTags::InAir;

	default:
		return FGameplayTag::EmptyTag;

	}
}