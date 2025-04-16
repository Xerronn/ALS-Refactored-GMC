#pragma once

#include "AlsRagdollingState.generated.h"

USTRUCT(BlueprintType)
struct ALS_API FAlsRagdollingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector TargetLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FRotator TargetRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bRagdolling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bResetMesh { false };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bFirstTick { false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector LastBonePosition { 0.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	double LastTime { 0 };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector LinearVelocity { 0.f };
};
