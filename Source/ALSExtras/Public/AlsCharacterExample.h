#pragma once

#include "AlsCharacter.h"
#include "AlsCharacterExample.generated.h"

struct FInputActionValue;
class UAlsCameraComponent;
class UInputMappingContext;
class UInputAction;

UCLASS(AutoExpandCategories = ("Settings|Als Character Example"))
class ALSEXTRAS_API AAlsCharacterExample : public AAlsCharacter
{
	GENERATED_BODY()

public:
	AAlsCharacterExample();

};
