/*******************************************************************************
The content of the files in this repository include portions of the
AUDIOKINETIC Wwise Technology released in source code form as part of the SDK
package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use these files in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Copyright (c) 2021 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include "Async/AsyncWork.h"
#include "Templates/SharedPointer.h"
#include "AssetManagement/WwiseProjectInfo.h"
#include "AssetManagement/AkSoundBankGenerationManager.h"

class AkGenerateSoundBanksTask : public FNonAbandonableTask
{
public:
	AkGenerateSoundBanksTask(const AkSoundBankGenerationManager::FInitParameters& InitParameters);
	~AkGenerateSoundBanksTask();

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(AkGenerateSoundBanksTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	static void ExecuteForEditorPlatform();
	static void CreateAndExecuteTask(const AkSoundBankGenerationManager::FInitParameters& InitParameters);

private:
	TSharedPtr<AkSoundBankGenerationManager, ESPMode::ThreadSafe> GenerationManager;
};
