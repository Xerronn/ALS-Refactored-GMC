#pragma once

#include "AlsTurnInPlaceState.generated.h"

class UAlsTurnInPlaceSettings;

USTRUCT(BlueprintType)
struct ALS_API FAlsTurnInPlaceState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	UAlsTurnInPlaceSettings* TurnInPlaceSettings{nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Meta = (ForceUnits = "s"))
	float ActivationDelay{0.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Meta = (ClampMin = 0, ClampMax = 180, ForceUnits = "deg"))
	float QueuedTurnYawAngle{0.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Meta = (ClampMin = 0, ForceUnits = "x"))
	float PlayRate{1.0f};
};
