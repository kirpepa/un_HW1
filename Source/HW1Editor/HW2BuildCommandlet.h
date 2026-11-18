#pragma once

#include "Commandlets/Commandlet.h"
#include "HW2BuildCommandlet.generated.h"

/** Reproducible authoring of the homework's editable assets. Never runs during normal play. */
UCLASS()
class UHW2BuildCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHW2BuildCommandlet();
    virtual int32 Main(const FString& Params) override;
};
