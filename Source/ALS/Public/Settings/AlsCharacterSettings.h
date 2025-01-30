#pragma once

#include "AlsInAirRotationMode.h"
#include "AlsMantlingSettings.h"
#include "AlsRagdollingSettings.h"
#include "AlsRollingSettings.h"
#include "AlsRotateInPlaceSettings.h"
#include "AlsTurnInPlaceSettings.h"
#include "AlsViewSettings.h"
#include "AlsCharacterSettings.generated.h"

UCLASS(Blueprintable, BlueprintType)
class ALS_API UAlsCharacterSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	// Actor will be considered as teleported if he has moved farther than this distance in 1 frame.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", Meta = (ClampMin = 0, ForceUnits = "cm"))
	float TeleportDistanceThreshold{50.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", Meta = (ClampMin = 0, ForceUnits = "cm/s"))
	float MovingSpeedThreshold{50.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	EAlsInAirRotationMode InAirRotationMode{EAlsInAirRotationMode::RotateToVelocityOnJump};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	uint8 bAllowAimingWhenInAir : 1 {true};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	uint8 bSprintHasPriorityOverAiming : 1 {false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	uint8 bRotateToVelocityWhenSprinting : 1 {false};

	// If checked, the character will automatically rotate towards the view direction on any movement input while the actor is not moving.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	uint8 bAutoRotateOnAnyInputWhileNotMovingInViewDirectionRotationMode : 1 {true};

	//ported from AlsAnimationInstance Settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsRotateInPlaceSettings RotateInPlace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsGeneralTurnInPlaceSettings TurnInPlace;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", Meta = (ClampMin = 0))
	float VelocityBlendInterpolationSpeed{12.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UCurveFloat> RotationYawOffsetForwardCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UCurveFloat> RotationYawOffsetBackwardCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UCurveFloat> RotationYawOffsetLeftCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TObjectPtr<UCurveFloat> RotationYawOffsetRightCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsViewSettings View;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsGeneralMantlingSettings Mantling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsRagdollingSettings Ragdolling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsRollingSettings Rolling;

public:
	UAlsCharacterSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& ChangedEvent) override;
#endif
};
