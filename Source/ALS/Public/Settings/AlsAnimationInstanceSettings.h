#pragma once

#include "AlsCrouchingSettings.h"
#include "AlsDynamicTransitionsSettings.h"
#include "AlsFeetSettings.h"
#include "AlsGeneralAnimationSettings.h"
#include "AlsGroundedSettings.h"
#include "AlsInAirSettings.h"
#include "AlsStandingSettings.h"
#include "AlsTransitionsSettings.h"
#include "AlsViewAnimationSettings.h"
#include "Engine/DataAsset.h"
#include "AlsAnimationInstanceSettings.generated.h"

UCLASS(Blueprintable, BlueprintType)
class ALS_API UAlsAnimationInstanceSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsGeneralAnimationSettings General;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsViewAnimationSettings View;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsGroundedSettings Grounded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsStandingSettings Standing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsCrouchingSettings Crouching;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsInAirSettings InAir;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsFeetSettings Feet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsTransitionsSettings Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAlsDynamicTransitionsSettings DynamicTransitions;

public:
	UAlsAnimationInstanceSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& ChangedEvent) override;
#endif
};
