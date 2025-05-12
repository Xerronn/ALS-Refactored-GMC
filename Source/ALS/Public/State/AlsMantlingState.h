#pragma once

#include "Settings/AlsMantlingSettings.h"

#include "AlsMantlingState.generated.h"

USTRUCT(BlueprintType)
struct ALS_API FAlsMantlingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	float MontageStartTime{0.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	float MantlingTimer{0.0f};

	//EAlsMantlingType enum as uint8 for binding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	uint8 MantlingType{0};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	TWeakObjectPtr<UPrimitiveComponent> TargetPrimitive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector TargetLocation{ForceInit};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FRotator TargetRotation{ForceInit};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector ActorFeetLocationOffset{ForceInit};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FRotator ActorRotationOffset{ForceInit};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FVector TargetAnimationLocation{ForceInit};
};
