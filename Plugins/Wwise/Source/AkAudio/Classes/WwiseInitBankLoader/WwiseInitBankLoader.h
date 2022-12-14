/*******************************************************************************
The content of the files in this repository include portions of the
AUDIOKINETIC Wwise Technology released in source code form as part of the SDK
package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use these files in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Copyright (c) 2022 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include "AkInitBank.h"
#include "Wwise/CookedData/WwiseInitBankCookedData.h"
#include "Subsystems/EngineSubsystem.h"


#include "WwiseInitBankLoader.generated.h"


UCLASS()
class UWwiseInitBankLoader : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	 static UWwiseInitBankLoader* Get()
	{
		if (UNLIKELY(!GEngine))
		{
			return nullptr;
		}
		return GEngine->GetEngineSubsystem<UWwiseInitBankLoader>();
	}

	UWwiseInitBankLoader();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool FindInitBank();
#if WITH_EDITORONLY_DATA
	void FindOrCreateInitBank();
#endif

	void LoadInitBank();
	void UnloadInitBank();
	void ReloadInitBank();
	UAkInitBank* GetInitBankAsset() const { return InitBankAsset; }
private:

	UPROPERTY()
	UAkInitBank* InitBankAsset;

};
