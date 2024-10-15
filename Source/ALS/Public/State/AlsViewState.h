#pragma once

#include "AlsViewState.generated.h"

USTRUCT(BlueprintType)
struct ALS_API FAlsViewState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FRotator Rotation{ForceInit};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Meta = (ClampMin = 0, ForceUnits = "deg/s"))
	float YawSpeed{0.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Meta = (ClampMin = -180, ClampMax = 180, ForceUnits = "deg"))
	float PreviousYawAngle{0.0f};
};
