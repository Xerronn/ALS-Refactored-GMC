#pragma once

#include "AlsRagdollingState.generated.h"

USTRUCT(BlueprintType)
struct ALS_API FAlsRagdollingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector TargetLocation{ForceInit};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FRotator TargetRotation{ForceInit};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bResetMesh{false};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bFirstTick{false};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector LinearVelocity{0.f};
};
