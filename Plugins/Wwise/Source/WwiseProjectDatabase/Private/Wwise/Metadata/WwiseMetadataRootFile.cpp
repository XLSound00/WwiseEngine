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

#include "Wwise/Metadata/WwiseMetadataRootFile.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Wwise/Metadata/WwiseMetadataPlatformInfo.h"
#include "Wwise/Metadata/WwiseMetadataPluginInfo.h"
#include "Wwise/Metadata/WwiseMetadataProjectInfo.h"
#include "Wwise/Metadata/WwiseMetadataSoundBanksInfo.h"
#include "Wwise/Metadata/WwiseMetadataLoader.h"
#include "Wwise/Stats/ProjectDatabase.h"

#include "AkUEFeatures.h"

FWwiseMetadataRootFile::FWwiseMetadataRootFile(FWwiseMetadataLoader& Loader) :
	PlatformInfo(Loader.GetObjectPtr<FWwiseMetadataPlatformInfo>(this, TEXT("PlatformInfo"))),
	PluginInfo(Loader.GetObjectPtr<FWwiseMetadataPluginInfo>(this, TEXT("PluginInfo"))),
	ProjectInfo(Loader.GetObjectPtr<FWwiseMetadataProjectInfo>(this, TEXT("ProjectInfo"))),
	SoundBanksInfo(Loader.GetObjectPtr<FWwiseMetadataSoundBanksInfo>(this, TEXT("SoundBanksInfo")))
{
	if (Loader.bResult && !PlatformInfo && !PluginInfo && !ProjectInfo && !SoundBanksInfo)
	{
		Loader.Fail(TEXT("FWwiseMetadataRootFile"));
	}
	IncLoadedSize(sizeof(FWwiseMetadataRootFile));
}

FWwiseMetadataRootFile::~FWwiseMetadataRootFile()
{
	if (PlatformInfo)
	{
		delete PluginInfo;
		PluginInfo = nullptr;
	}
	if (PluginInfo)
	{
		delete PluginInfo;
		PluginInfo = nullptr;
	}
	if (ProjectInfo)
	{
		delete PluginInfo;
		PluginInfo = nullptr;
	}
	if (SoundBanksInfo)
	{
		delete SoundBanksInfo;
		SoundBanksInfo = nullptr;
	}
}

class FWwiseAsyncLoadFileTask : public FNonAbandonableTask
{
	friend class FAsyncTask<FWwiseAsyncLoadFileTask>;

	WwiseMetadataSharedRootFilePtr& Output;
	const FString& FilePath;

public:
	FWwiseAsyncLoadFileTask(
		WwiseMetadataSharedRootFilePtr& OutputParam,
		const FString& FilePathParam) :
		Output(OutputParam),
		FilePath(FilePathParam)
	{
	}

protected:
	void DoWork()
	{
		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
		{
			UE_LOG(LogWwiseProjectDatabase, Error, TEXT("Error while loading file %s to string"), *FilePath);
			return;
		}

		Output = FWwiseMetadataRootFile::LoadFile(MoveTemp(FileContents), *FilePath);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FWwiseAsyncLoadFileTask, STATGROUP_WwiseProjectDatabase);
	}
};

WwiseMetadataSharedRootFilePtr FWwiseMetadataRootFile::LoadFile(FString&& File, const FString& FilePath)
{
	UE_LOG(LogWwiseProjectDatabase, Verbose, TEXT("Parsing file in: %s"), *FilePath);

	auto JsonReader = TJsonReaderFactory<>::Create(MoveTemp(File));
	TSharedPtr<FJsonObject> RootJsonObject;
	if (!FJsonSerializer::Deserialize(JsonReader, RootJsonObject))
	{
		UE_LOG(LogWwiseProjectDatabase, Error, TEXT("Error while decoding json"));
		return {};
	}

	FWwiseMetadataLoader Loader(RootJsonObject.ToSharedRef());
	auto Result = MakeShared<FWwiseMetadataRootFile>(Loader);

	if (!Loader.bResult)
	{
		Loader.LogParsed(TEXT("LoadFile"), 0, *FilePath);
		return {};
	}

	return Result;
}

WwiseMetadataSharedRootFilePtr FWwiseMetadataRootFile::LoadFile(const FString& FilePath)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
	{
		UE_LOG(LogWwiseProjectDatabase, Error, TEXT("Error while loading file %s to string"), *FilePath);
		return nullptr;
	}

	return LoadFile(MoveTemp(FileContents), FilePath);
}

WwiseMetadataFileMap FWwiseMetadataRootFile::LoadFiles(const TArray<FString>& FilePaths)
{
	WwiseMetadataFileMap Result;
	for (const auto& FilePath : FilePaths)
	{
		Result.Add(FilePath, {});
	}

	TArray<FAsyncTask<FWwiseAsyncLoadFileTask>> Tasks;
	Tasks.Empty(Result.Num());

	for (auto& Elem : Result)
	{
		Tasks.Emplace(Elem.Value, Elem.Key);
	}

	for (auto& Task : Tasks)
	{
		Task.StartBackgroundTask();
	}

	for (auto& Task : Tasks)
	{
#if UE_5_0_OR_LATER
		Task.EnsureCompletion(false, true);		// Condemn that thread until Root is done. Otherwise, A might wait for B waiting for A.
#else
		Task.EnsureCompletion(false);		// Condemn that thread until Root is done. Otherwise, A might wait for B waiting for A.
#endif
	}

	return Result;
}
