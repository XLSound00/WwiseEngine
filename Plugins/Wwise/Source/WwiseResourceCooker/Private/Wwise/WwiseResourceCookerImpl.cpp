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

#include "Wwise/WwiseResourceCookerImpl.h"

#include "Wwise/WwiseExternalSourceManager.h"
#include "Wwise/WwiseResourceLoader.h"
#include "Wwise/WwiseCookingCache.h"
#include "Wwise/Metadata/WwiseMetadataPlatformInfo.h"
#include "Wwise/Metadata/WwiseMetadataPlugin.h"
#include "Wwise/Stats/ResourceCooker.h"

#include "Async/Async.h"
#include "Async/MappedFileHandle.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Wwise/CookedData/WwiseSoundBankCookedData.h"
#include "Wwise/Stats/ResourceCooker.h"

UWwiseResourceCookerImpl::UWwiseResourceCookerImpl() :
	ExportDebugNameRule(EWwiseExportDebugNameRule::ObjectPath),
	CookingCache(nullptr),
	ProjectDatabaseOverride(nullptr)
{
}

UWwiseResourceCookerImpl::~UWwiseResourceCookerImpl()
{
}

UWwiseProjectDatabase* UWwiseResourceCookerImpl::GetProjectDatabase()
{
	if (ProjectDatabaseOverride)
	{
		return ProjectDatabaseOverride;
	}
	else
	{
		return UWwiseProjectDatabase::Get();
	}
}

const UWwiseProjectDatabase* UWwiseResourceCookerImpl::GetProjectDatabase() const
{
	if (ProjectDatabaseOverride)
	{
		return ProjectDatabaseOverride;
	}
	else
	{
		return UWwiseProjectDatabase::Get();
	}
}

void UWwiseResourceCookerImpl::PrepareResourceCookerForPlatform(UWwiseProjectDatabase* InProjectDatabaseOverride, EWwiseExportDebugNameRule InExportDebugNameRule)
{
	ProjectDatabaseOverride = InProjectDatabaseOverride;
	ExportDebugNameRule = InExportDebugNameRule;
	CookingCache = NewObject<UWwiseCookingCache>();
	CookingCache->ExternalSourceManager = IWwiseExternalSourceManager::Get();
}



void UWwiseResourceCookerImpl::CookAuxBusToSandbox(const FWwiseAuxBusCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking AuxBus %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.AuxBusId);
	for (const auto& SoundBank : InCookedData.SoundBanks)
	{
		CookSoundBankToSandbox(SoundBank, WriteAdditionalFile);
	}
	for (const auto& Media : InCookedData.Media)
	{
		CookMediaToSandbox(Media, WriteAdditionalFile);
	}
	UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("Done cooking AuxBus %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.AuxBusId);
}

void UWwiseResourceCookerImpl::CookEventToSandbox(const FWwiseEventCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking Event %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.EventId);
	for (const auto& SoundBank : InCookedData.SoundBanks)
	{
		CookSoundBankToSandbox(SoundBank, WriteAdditionalFile);
	}
	for (const auto& Media : InCookedData.Media)
	{
		CookMediaToSandbox(Media, WriteAdditionalFile);
	}
	for (const auto& ExternalSource : InCookedData.ExternalSources)
	{
		CookExternalSourceToSandbox(ExternalSource, WriteAdditionalFile);
	}
	for (const auto& SwitchContainerLeaf : InCookedData.SwitchContainerLeaves)
	{
		UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking Event %s %" PRIu32 " Switched Media"), *InCookedData.DebugName, (uint32)InCookedData.EventId);
		for (const auto& SoundBank : SwitchContainerLeaf.SoundBanks)
		{
			CookSoundBankToSandbox(SoundBank, WriteAdditionalFile);
		}
		for (const auto& Media : SwitchContainerLeaf.Media)
		{
			CookMediaToSandbox(Media, WriteAdditionalFile);
		}
		for (const auto& ExternalSource : SwitchContainerLeaf.ExternalSources)
		{
			CookExternalSourceToSandbox(ExternalSource, WriteAdditionalFile);
		}
	}
	UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("Done cooking Event %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.EventId);
}

void UWwiseResourceCookerImpl::CookExternalSourceToSandbox(const FWwiseExternalSourceCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	if (LIKELY(CookingCache && CookingCache->ExternalSourceManager))
	{
		CookingCache->ExternalSourceManager->Cook(*this, InCookedData, WriteAdditionalFile, GetCurrentPlatform(), GetCurrentLanguage());
	}
	else
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("No External Source Manager while cooking External Source %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.Cookie);
	}
}

void UWwiseResourceCookerImpl::CookInitBankToSandbox(const FWwiseInitBankCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking Init SoundBank %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SoundBankId);
	CookSoundBankToSandbox(InCookedData, WriteAdditionalFile);

	for (const auto& Media : InCookedData.Media)
	{
		CookMediaToSandbox(Media, WriteAdditionalFile);
	}
	UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("Done cooking Init SoundBank %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SoundBankId);
}

void UWwiseResourceCookerImpl::CookMediaToSandbox(const FWwiseMediaCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking Media %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.MediaId);

	if (UNLIKELY(InCookedData.MediaPathName.IsEmpty()))
	{
		UE_LOG(LogWwiseResourceCooker, Fatal, TEXT("Empty pathname for Media %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.MediaId);
		return;
	}

	auto* ResourceLoader = GetResourceLoader();
	if (UNLIKELY(!ResourceLoader))
	{
		return;
	}
	const FString GeneratedSoundBanksPath = ResourceLoader->GetUnrealGeneratedSoundBanksPath(InCookedData.MediaPathName);

	CookFileToSandbox(GeneratedSoundBanksPath, InCookedData.MediaPathName, WriteAdditionalFile);
}

void UWwiseResourceCookerImpl::CookSharesetToSandbox(const FWwiseSharesetCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking Shareset %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SharesetId);
	for (const auto& SoundBank : InCookedData.SoundBanks)
	{
		CookSoundBankToSandbox(SoundBank, WriteAdditionalFile);
	}
	for (const auto& Media : InCookedData.Media)
	{
		CookMediaToSandbox(Media, WriteAdditionalFile);
	}
	UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("Done cooking Shareset %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SharesetId);
}

void UWwiseResourceCookerImpl::CookSoundBankToSandbox(const FWwiseSoundBankCookedData& InCookedData, WriteAdditionalFileFunction WriteAdditionalFile)
{
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("Cooking SoundBank %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SoundBankId);

	if (UNLIKELY(InCookedData.SoundBankPathName.IsEmpty()))
	{
		UE_LOG(LogWwiseResourceCooker, Fatal, TEXT("Empty pathname for SoundBank %s %" PRIu32), *InCookedData.DebugName, (uint32)InCookedData.SoundBankId);
		return;
	}

	auto* ResourceLoader = GetResourceLoader();
	if (UNLIKELY(!ResourceLoader))
	{
		return;
	}
	const FString GeneratedSoundBanksPath = ResourceLoader->GetUnrealGeneratedSoundBanksPath(InCookedData.SoundBankPathName);

	CookFileToSandbox(GeneratedSoundBanksPath, InCookedData.SoundBankPathName, WriteAdditionalFile);
}

void UWwiseResourceCookerImpl::CookFileToSandbox(const FString& InInputPathName, const FString& InOutputPathName, WriteAdditionalFileFunction WriteAdditionalFile, bool bInStageRelativeToContent)
{
	auto* ResourceLoader = GetResourceLoader();
	if (UNLIKELY(!ResourceLoader))
	{
		return;
	}

	FString StagePath = bInStageRelativeToContent
		? SandboxRootPath / InOutputPathName
		: SandboxRootPath / ResourceLoader->GetUnrealStagePath(InOutputPathName);
	auto& StageFiles = CookingCache->StagedFiles;

	if (const auto* AlreadyStaged = StageFiles.Find(StagePath))
	{
		if (*AlreadyStaged == InInputPathName)
		{
			UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("Cook: Skipping already present file %s -> %s"), *InInputPathName, *StagePath);
		}
		else
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("Cook: Trying to stage two different files to the same path: [%s and %s] -> %s"), *InInputPathName, **AlreadyStaged, *StagePath);
		}

		return;
	}
	StageFiles.Add(StagePath, InInputPathName);

	if (auto* MappedHandle = FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*InInputPathName))
	{
		auto* MappedRegion = MappedHandle->MapRegion();
		if (MappedRegion)
		{
			UE_LOG(LogWwiseResourceCooker, Display, TEXT("Adding file %s [%" PRIi64 " bytes]"), *StagePath, MappedRegion->GetMappedSize());
			WriteAdditionalFile(*StagePath, (void*)MappedRegion->GetMappedPtr(), MappedRegion->GetMappedSize());
			delete MappedRegion;
			delete MappedHandle;
			return;
		}
		else
		{
			delete MappedHandle;
		}
	}

	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *InInputPathName))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("Cook: Could not read file %s"), *InInputPathName);
		return;
	}

	UE_LOG(LogWwiseResourceCooker, Display, TEXT("Adding file %s [%" PRIi64 " bytes]"), *StagePath, (int64)Data.Num());
	WriteAdditionalFile(*StagePath, (void*)Data.GetData(), Data.Num());
}

bool UWwiseResourceCookerImpl::GetAcousticTextureCookedData(FWwiseAcousticTextureCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAcousticTextureCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAcousticTextureCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	FWwiseRefAcousticTexture AcousticTextureRef;

	if (UNLIKELY(!PlatformData->GetRef(AcousticTextureRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAcousticTextureCookedData (%s %" PRIu32 " %s): No acoustic texture data found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const auto* AcousticTexture = AcousticTextureRef.GetAcousticTexture();

	OutCookedData.ShortId = AcousticTexture->Id;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutCookedData.DebugName.Empty();
	}
	else
	{
		OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? AcousticTexture->Name : AcousticTexture->ObjectPath;
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetAuxBusCookedData(FWwiseLocalizedAuxBusCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const auto* PlatformInfo = PlatformData->PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo)) return false;

	const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();

	TMap<FWwiseSharedLanguageId, FWwiseRefAuxBus> RefLanguageMap;
	PlatformData->GetRefMap(RefLanguageMap, Languages, InInfo);
	if (UNLIKELY(RefLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): No ref found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.AuxBusLanguageMap.Empty(RefLanguageMap.Num());

	for (const auto& Ref : RefLanguageMap)
	{
		FWwiseAuxBusCookedData CookedData;
		const auto* AuxBus = Ref.Value.GetAuxBus();
		if (UNLIKELY(!AuxBus))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): Could not get AuxBus from Ref"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			continue;
		}
		CookedData.AuxBusId = AuxBus->Id;

		TSet<const FWwiseRefAuxBus*> SubAuxBusRefs;
		Ref.Value.GetAllAuxBusRefs(SubAuxBusRefs, PlatformData->AuxBusses);
		TSet<FWwiseSoundBankCookedData> SoundBankSet;
		TSet<FWwiseMediaCookedData> MediaSet;
		for (const auto* SubAuxBusRef : SubAuxBusRefs)
		{
			const auto* SoundBank = SubAuxBusRef->GetSoundBank();
			if (UNLIKELY(!SoundBank))
			{
				UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				continue;
			}

			if (!SoundBank->IsInitBank())
			{
				FWwiseSoundBankCookedData SoundBankCookedData;
				if (UNLIKELY(!FillSoundBankBaseInfo(SoundBankCookedData, *PlatformInfo, *SoundBank)))
				{
					UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					continue;
				}
				SoundBankSet.Add(SoundBankCookedData);
			}

			{
				WwiseMediaIdsMap MediaRefs = SubAuxBusRef->GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, Ref.Key, *PlatformData)))
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): Could not fill Sub Media from Data"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						continue;
					}
				}
			}

			{
				WwiseCustomPluginIdsMap CustomPluginsRefs = SubAuxBusRef->GetCustomPlugins(PlatformData->CustomPlugins);
				for (const auto& Plugin : CustomPluginsRefs)
				{
					const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
					for (const auto& MediaRef : MediaRefs)
					{
						if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
						{
							return false;
						}
					}
				}
			}

			{
				WwisePluginSharesetIdsMap ShareSetRefs = SubAuxBusRef->GetPluginSharesets(PlatformData->PluginSharesets);
				for (const auto& ShareSet : ShareSetRefs)
				{
					const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
					for (const auto& MediaRef : MediaRefs)
					{
						if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
						{
							return false;
						}
					}
				}
			}

			{
				WwiseAudioDeviceIdsMap AudioDevicesRefs = SubAuxBusRef->GetAudioDevices(PlatformData->AudioDevices);
				for (const auto& AudioDevice : AudioDevicesRefs)
				{
					const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
					for (const auto& MediaRef : MediaRefs)
					{
						if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
						{
							return false;
						}
					}
				}
			}
		}

		CookedData.SoundBanks = SoundBankSet.Array();
		CookedData.Media = MediaSet.Array();

		if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
		{
			OutCookedData.DebugName.Empty();
		}
		else
		{
			CookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? AuxBus->Name : AuxBus->ObjectPath;
			OutCookedData.DebugName = CookedData.DebugName;
		}
		OutCookedData.AuxBusId = CookedData.AuxBusId;

		OutCookedData.AuxBusLanguageMap.Add(FWwiseLanguageCookedData(Ref.Key.GetLanguageId(), Ref.Key.GetLanguageName(), Ref.Key.LanguageRequirement), MoveTemp(CookedData));
	}

	if (UNLIKELY(OutCookedData.AuxBusLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): No AuxBus"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	// Make this a SFX if all CookedData are identical
	{
		auto& Map = OutCookedData.AuxBusLanguageMap;
		TArray<FWwiseLanguageCookedData> Keys;
		Map.GetKeys(Keys);

		auto LhsKey = Keys.Pop(false);
		const auto* Lhs = Map.Find(LhsKey);
		while (Keys.Num() > 0)
		{
			auto RhsKey = Keys.Pop(false);
			const auto* Rhs = Map.Find(RhsKey);

			if (Lhs->AuxBusId != Rhs->AuxBusId
				|| Lhs->DebugName != Rhs->DebugName
				|| Lhs->SoundBanks.Num() != Rhs->SoundBanks.Num()
				|| Lhs->Media.Num() != Rhs->Media.Num())
			{
				UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): AuxBus has languages"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return true;
			}
			for (const auto& Elem : Lhs->SoundBanks)
			{
				if (!Rhs->SoundBanks.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): AuxBus has languages due to banks"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& Elem : Lhs->Media)
			{
				if (!Rhs->Media.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): AuxBus has languages due to media"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
		}

		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetAuxBusCookedData (%s %" PRIu32 " %s): AuxBus is a SFX"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		std::remove_reference_t<decltype(Map)> SfxMap;
		SfxMap.Add(FWwiseLanguageCookedData::Sfx, *Lhs);

		Map = SfxMap;
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetEventCookedData(FWwiseLocalizedEventCookedData& OutCookedData, const FWwiseEventInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();

	const auto* PlatformInfo = PlatformData->PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): No Platform Info"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	TMap<FWwiseSharedLanguageId, TSet<FWwiseRefEvent>> RefLanguageMap;
	PlatformData->GetRefMap(RefLanguageMap, Languages, InInfo);
	if (UNLIKELY(RefLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): No ref found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.EventLanguageMap.Empty(RefLanguageMap.Num());
	UE_LOG(LogWwiseResourceCooker, Verbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Adding %d languages to map"),
		*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName, RefLanguageMap.Num());

	for (auto& Ref : RefLanguageMap)
	{
		FWwiseEventCookedData CookedData;

		TSet<FWwiseSoundBankCookedData> SoundBankSet;
		TSet<FWwiseMediaCookedData> MediaSet;

		const FWwiseSharedLanguageId& LanguageId = Ref.Key;
		TSet<FWwiseRefEvent>& Events = Ref.Value;
		WwiseSwitchContainerArray SwitchContainerRefs;

		if (UNLIKELY(Events.Num() == 0))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Empty ref for language"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}

		// Set up basic global Event information
		{
			TSet<FWwiseRefEvent>::TConstIterator FirstEvent(Events);
			CookedData.EventId = FirstEvent->EventId();
			if (ExportDebugNameRule != EWwiseExportDebugNameRule::Release)
			{
				CookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? FirstEvent->EventName() : FirstEvent->EventObjectPath();
				OutCookedData.DebugName = CookedData.DebugName;
			}

			OutCookedData.EventId = CookedData.EventId;
			SwitchContainerRefs = FirstEvent->GetSwitchContainers(PlatformData->SwitchContainersByEvent);
		}

		// Add extra events recursively
		{
			TSet<FWwiseRefEvent> DiffEvents = Events;
			while (true)
			{
				bool bHaveMore = false;
				TSet<FWwiseRefEvent> OldEvents(Events);
				for (auto& EventRef : OldEvents)
				{
					const FWwiseMetadataEvent* Event = EventRef.GetEvent();
					if (UNLIKELY(!Event))
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get Event from Ref"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return false;
					}

					for (const auto& ActionPostEvent : Event->ActionPostEvent)
					{
						bool bHaveMoreInThisEvent = PlatformData->GetRef(Events, LanguageId, FWwiseEventInfo(ActionPostEvent.Id, ActionPostEvent.Name));
						bHaveMore = bHaveMore || bHaveMoreInThisEvent;
					}
				}
				if (bHaveMore)
				{
					DiffEvents = Events.Difference(OldEvents);
					if (DiffEvents.Num() == 0)
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): GetRef should return false when no more additional Refs"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						break;
					}
					for (auto& EventRef : DiffEvents)
					{
						SwitchContainerRefs.Append(EventRef.GetSwitchContainers(PlatformData->SwitchContainersByEvent));
					}
				}
				else
				{
					break;
				}
			}
		}

		// Add mandatory SoundBank information
		TSet<FWwiseExternalSourceCookedData> ExternalSourceSet;
		TSet<FWwiseAnyRef> RequiredGroupValueSet;
		for (auto& EventRef : Events)
		{
			const FWwiseMetadataEvent* Event = EventRef.GetEvent();
			if (UNLIKELY(!Event))
			{
				UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get Event from Ref"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return false;
			}

			if (LIKELY(Event->IsMandatory()) || LIKELY(Events.Num() == 1))
			{
				// Add main SoundBank
				{
					const auto* SoundBank = EventRef.GetSoundBank();
					if (UNLIKELY(!SoundBank))
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return false;
					}
					if (!SoundBank->IsInitBank())
					{
						FWwiseSoundBankCookedData MainSoundBank;
						if (UNLIKELY(!FillSoundBankBaseInfo(MainSoundBank, *PlatformInfo, *SoundBank)))
						{
							UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
								*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
							return false;
						}
						SoundBankSet.Add(MainSoundBank);
					}
				}

				// Get Aux Bus banks & media
				{
					WwiseAuxBusIdsMap AuxBusRefs = EventRef.GetAuxBusses(PlatformData->AuxBusses);
					TSet<const FWwiseRefAuxBus*> SubAuxBusRefs;
					for (const auto& AuxBusRef : AuxBusRefs)
					{
						AuxBusRef.Value.GetAllAuxBusRefs(SubAuxBusRefs, PlatformData->AuxBusses);
					}
					for (const auto* SubAuxBusRef : SubAuxBusRefs)
					{
						const auto* SoundBank = SubAuxBusRef->GetSoundBank();
						if (UNLIKELY(!SoundBank))
						{
							UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
								*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
							return false;
						}

						if (!SoundBank->IsInitBank())
						{
							FWwiseSoundBankCookedData SoundBankCookedData;
							if (UNLIKELY(!FillSoundBankBaseInfo(SoundBankCookedData, *PlatformInfo, *SoundBank)))
							{
								UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
									*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
								return false;
							}
							SoundBankSet.Add(SoundBankCookedData);
						}

						{
							WwiseMediaIdsMap MediaRefs = SubAuxBusRef->GetMedia(PlatformData->MediaFiles);
							for (const auto& MediaRef : MediaRefs)
							{
								if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
								{
									UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not fill Sub Media from Data"),
										*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
									return false;
								}
							}
						}

						{
							WwiseCustomPluginIdsMap CustomPluginsRefs = SubAuxBusRef->GetCustomPlugins(PlatformData->CustomPlugins);
							for (const auto& Plugin : CustomPluginsRefs)
							{
								const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
								for (const auto& MediaRef : MediaRefs)
								{
									if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
									{
										return false;
									}
								}
							}
						}

						{
							WwisePluginSharesetIdsMap ShareSetRefs = SubAuxBusRef->GetPluginSharesets(PlatformData->PluginSharesets);
							for (const auto& ShareSet : ShareSetRefs)
							{
								const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
								for (const auto& MediaRef : MediaRefs)
								{
									if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
									{
										return false;
									}
								}
							}
						}

						{
							WwiseAudioDeviceIdsMap AudioDevicesRefs = SubAuxBusRef->GetAudioDevices(PlatformData->AudioDevices);
							for (const auto& AudioDevice : AudioDevicesRefs)
							{
								const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
								for (const auto& MediaRef : MediaRefs)
								{
									if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
									{
										return false;
									}
								}
							}
						}
					}
				}

				// Get media
				{
					WwiseMediaIdsMap MediaRefs = EventRef.GetMedia(PlatformData->MediaFiles);
					for (const auto& MediaRef : MediaRefs)
					{
						if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
						{
							return false;
						}
					}
				}

				// Get Media from custom plugins
				{
					WwiseCustomPluginIdsMap CustomPluginsRefs = EventRef.GetCustomPlugins(PlatformData->CustomPlugins);
					for (const auto& Plugin : CustomPluginsRefs)
					{
						const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}

				// Get Media from plugin ShareSets
				{
					WwisePluginSharesetIdsMap ShareSetRefs = EventRef.GetPluginSharesets(PlatformData->PluginSharesets);
					for (const auto& ShareSet : ShareSetRefs)
					{
						const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}

				// Get Media from audio devices
				{
					WwiseAudioDeviceIdsMap AudioDevicesRefs = EventRef.GetAudioDevices(PlatformData->AudioDevices);
					for (const auto& AudioDevice : AudioDevicesRefs)
					{
						const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}

				// Get External Sources
				{
					WwiseExternalSourceIdsMap ExternalSourceRefs = EventRef.GetExternalSources(PlatformData->ExternalSources);
					for (const auto& ExternalSourceRef : ExternalSourceRefs)
					{
						if (UNLIKELY(!AddRequirementsForExternalSource(ExternalSourceSet, ExternalSourceRef.Value)))
						{
							return false;
						}
					}
				}

				// Get required GroupValues
				{
					for (const auto& Switch : EventRef.GetActionSetSwitch(PlatformData->Switches))
					{
						if (LIKELY(Switch.Value.IsValid()))
						{
							RequiredGroupValueSet.Add(FWwiseAnyRef::Create(Switch.Value));
						}
					}

					for (const auto& State : EventRef.GetActionSetState(PlatformData->States))
					{
						if (LIKELY(State.Value.IsValid()))
						{
							RequiredGroupValueSet.Add(FWwiseAnyRef::Create(State.Value));
						}
					}
				}
			}
		}

		// Get Switched Media, negating required switches.
		{
			TMap<FWwiseRefSwitchContainer, TSet<FWwiseAnyRef>> SwitchValuesMap;

			for (const auto& SwitchContainerRef : SwitchContainerRefs)
			{
				const auto* SwitchContainer = SwitchContainerRef.GetSwitchContainer();
				if (UNLIKELY(!SwitchContainer))
				{
					return false;
				}

				auto SwitchValues = TSet<FWwiseAnyRef>(SwitchContainerRef.GetSwitchValues(PlatformData->Switches, PlatformData->States));

				TSet<FWwiseAnyRef> SwitchesToRemove;
				for (const auto& SwitchValue : SwitchValues)
				{
					// Remove all SwitchValues if we load them all by default
					if (InInfo.SwitchContainerLoading == EWwiseEventSwitchContainerLoading::AlwaysLoad)
					{
						SwitchesToRemove.Add(SwitchValue);
						continue;
					}

					// Remove SwitchValues that are already present in RequiredGroupValueSet
					if (RequiredGroupValueSet.Contains(SwitchValue))
					{
						SwitchesToRemove.Add(SwitchValue);
						continue;
					}

					// Remove SwitchValues that have an ID of "0" (wildcard in music)
					if (SwitchValue.GetId() == 0)
					{
						SwitchesToRemove.Add(SwitchValue);
						continue;
					}

					// Remove Switch groups that are controlled by a Game Parameter (RTPC)
					if (SwitchValue.GetType() == EWwiseRefType::Switch)
					{
						const auto* SwitchRef = SwitchValue.GetSwitchRef();
						check(SwitchRef);

						if (SwitchRef->IsControlledByGameParameter())
						{
							SwitchesToRemove.Add(SwitchValue);
							continue;
						}
					}
				}
				SwitchValues = SwitchValues.Difference(SwitchesToRemove);

				if (SwitchValues.Num() == 0)
				{
					// Media and SoundBank are compulsory. Add them so they are always loaded.
					const auto* SoundBank = SwitchContainerRef.GetSoundBank();
					if (UNLIKELY(!SoundBank))
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Switch Container Ref"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return false;
					}
					if (!SoundBank->IsInitBank())
					{
						FWwiseSoundBankCookedData SwitchContainerSoundBank;
						if (UNLIKELY(!FillSoundBankBaseInfo(SwitchContainerSoundBank, *PlatformInfo, *SoundBank)))
						{
							UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Switch Container Data"),
								*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
							return false;
						}
						SoundBankSet.Add(SwitchContainerSoundBank);
					}

					{
						TArray<FWwiseRefMedia> MediaToAdd;
						SwitchContainerRef.GetMedia(PlatformData->MediaFiles).GenerateValueArray(MediaToAdd);
						for (const auto& MediaRef : MediaToAdd)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef, Ref.Key, *PlatformData)))
							{
								return false;
							}
						}
					}

					{
						WwiseCustomPluginIdsMap CustomPluginsRefs = SwitchContainerRef.GetCustomPlugins(PlatformData->CustomPlugins);
						for (const auto& Plugin : CustomPluginsRefs)
						{
							const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
							for (const auto& MediaRef : MediaRefs)
							{
								if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
								{
									return false;
								}
							}
						}
					}

					{
						WwisePluginSharesetIdsMap ShareSetRefs = SwitchContainerRef.GetPluginSharesets(PlatformData->PluginSharesets);
						for (const auto& ShareSet : ShareSetRefs)
						{
							const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
							for (const auto& MediaRef : MediaRefs)
							{
								if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
								{
									return false;
								}
							}
						}
					}

					{
						WwiseAudioDeviceIdsMap AudioDevicesRefs = SwitchContainerRef.GetAudioDevices(PlatformData->AudioDevices);
						for (const auto& AudioDevice : AudioDevicesRefs)
						{
							const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
							for (const auto& MediaRef : MediaRefs)
							{
								if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
								{
									return false;
								}
							}
						}
					}

					TArray<FWwiseRefExternalSource> ExternalSourcesToAdd;
					SwitchContainerRef.GetExternalSources(PlatformData->ExternalSources).GenerateValueArray(ExternalSourcesToAdd);

					for (const auto& ExternalSourceRef : ExternalSourcesToAdd)
					{
						if (UNLIKELY(!AddRequirementsForExternalSource(ExternalSourceSet, ExternalSourceRef)))
						{
							return false;
						}
					}
				}
				else
				{
					// Media is optional. Will process later
					SwitchValuesMap.Add(SwitchContainerRef, SwitchValues);
				}
			}

			// Process Switch Containers that seemingly contain additional media and conditions
			for (const auto& SwitchContainerRef : SwitchContainerRefs)
			{
				const auto* SwitchValues = SwitchValuesMap.Find(SwitchContainerRef);
				if (!SwitchValues)
				{
					continue;
				}

				// Prepare media and main SoundBank to add
				TSet<FWwiseSoundBankCookedData> SoundBankSetToAdd;
				TSet<FWwiseMediaCookedData> MediaSetToAdd;
				TSet<FWwiseExternalSourceCookedData> ExternalSourceSetToAdd;

				const auto* SoundBank = SwitchContainerRef.GetSoundBank();
				if (UNLIKELY(!SoundBank))
				{
					UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Switch Container Ref"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return false;
				}
				if (!SoundBank->IsInitBank())
				{
					FWwiseSoundBankCookedData SwitchContainerSoundBank;
					if (UNLIKELY(!FillSoundBankBaseInfo(SwitchContainerSoundBank, *PlatformInfo, *SoundBank)))
					{
						UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Switch Container Data"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return false;
					}
					SoundBankSetToAdd.Add(SwitchContainerSoundBank);
				}

				{
					TArray<FWwiseRefMedia> MediaToAdd;
					SwitchContainerRef.GetMedia(PlatformData->MediaFiles).GenerateValueArray(MediaToAdd);
					FWwiseMediaCookedData MediaCookedData;
					for (const auto& MediaRef : MediaToAdd)
					{
						if (UNLIKELY(!AddRequirementsForMedia(SoundBankSetToAdd, MediaSetToAdd, MediaRef, Ref.Key, *PlatformData)))
						{
							return false;
						}
					}
				}

				{
					WwiseCustomPluginIdsMap CustomPluginsRefs = SwitchContainerRef.GetCustomPlugins(PlatformData->CustomPlugins);
					for (const auto& Plugin : CustomPluginsRefs)
					{
						const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}

				{
					WwisePluginSharesetIdsMap ShareSetRefs = SwitchContainerRef.GetPluginSharesets(PlatformData->PluginSharesets);
					for (const auto& ShareSet : ShareSetRefs)
					{
						const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}

				{
					WwiseAudioDeviceIdsMap AudioDevicesRefs = SwitchContainerRef.GetAudioDevices(PlatformData->AudioDevices);
					for (const auto& AudioDevice : AudioDevicesRefs)
					{
						const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
						for (const auto& MediaRef : MediaRefs)
						{
							if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, LanguageId, *PlatformData)))
							{
								return false;
							}
						}
					}
				}


				TArray<FWwiseRefExternalSource> ExternalSourcesToAdd;
				SwitchContainerRef.GetExternalSources(PlatformData->ExternalSources).GenerateValueArray(ExternalSourcesToAdd);

				for (const auto& ExternalSourceRef : ExternalSourcesToAdd)
				{
					if (UNLIKELY(!AddRequirementsForExternalSource(ExternalSourceSetToAdd, ExternalSourceRef)))
					{
						return false;
					}
				}

				SoundBankSetToAdd = SoundBankSetToAdd.Difference(SoundBankSet);
				MediaSetToAdd = MediaSetToAdd.Difference(MediaSet);
				ExternalSourceSetToAdd = ExternalSourceSetToAdd.Difference(ExternalSourceSet);

				// Have we already included all the external banks and media
				if (SoundBankSetToAdd.Num() == 0 && MediaSetToAdd.Num() == 0 && ExternalSourceSetToAdd.Num() == 0)
				{
					continue;
				}

				// Fill up SwitchContainerCookedData and add it to SwitchContainerLeaves
				FWwiseSwitchContainerLeafCookedData SwitchContainerCookedData;
				for (const auto& SwitchValue : *SwitchValues)
				{
					FWwiseGroupValueCookedData SwitchCookedData;
					switch (SwitchValue.GetType())
					{
					case EWwiseRefType::Switch: SwitchCookedData.Type = EWwiseGroupType::Switch; break;
					case EWwiseRefType::State: SwitchCookedData.Type = EWwiseGroupType::State; break;
					default: SwitchCookedData.Type = EWwiseGroupType::Unknown;
					}
					SwitchCookedData.GroupId = SwitchValue.GetGroupId();
					SwitchCookedData.Id = SwitchValue.GetId();
					if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
					{
						SwitchCookedData.DebugName.Empty();
					}
					else
					{
						SwitchCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? SwitchValue.GetName() : SwitchValue.GetObjectPath();
					}
					SwitchContainerCookedData.GroupValueSet.Add(MoveTemp(SwitchCookedData));
				}
				if (auto* ExistingSwitchedMedia = CookedData.SwitchContainerLeaves.FindByPredicate([&SwitchContainerCookedData](const FWwiseSwitchContainerLeafCookedData& RhsValue)
				{
					return RhsValue.GroupValueSet.Difference(SwitchContainerCookedData.GroupValueSet).Num() == 0;
				}))
				{
					SoundBankSetToAdd.Append(ExistingSwitchedMedia->SoundBanks);
					MediaSetToAdd.Append(ExistingSwitchedMedia->Media);
					ExternalSourceSetToAdd.Append(ExistingSwitchedMedia->ExternalSources);

					ExistingSwitchedMedia->SoundBanks = SoundBankSetToAdd.Array();
					ExistingSwitchedMedia->Media = MediaSetToAdd.Array();
					ExistingSwitchedMedia->ExternalSources = ExternalSourceSetToAdd.Array();
				}
				else
				{
					SwitchContainerCookedData.SoundBanks = SoundBankSetToAdd.Array();
					SwitchContainerCookedData.Media = MediaSetToAdd.Array();
					SwitchContainerCookedData.ExternalSources = ExternalSourceSetToAdd.Array();
					CookedData.SwitchContainerLeaves.Add(MoveTemp(SwitchContainerCookedData));
				}
			}
		}

		// Finalize banks and media
		CookedData.SoundBanks.Append(SoundBankSet.Array());
		if (CookedData.SoundBanks.Num() == 0)
		{
			UE_LOG(LogWwiseResourceCooker, Log, TEXT("GetEventCookedData (%s %" PRIu32 " %s): No SoundBank set for Event. Unless Switch values are properly set, no SoundBank will be loaded."),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		}
		CookedData.Media.Append(MediaSet.Array());
		CookedData.ExternalSources.Append(ExternalSourceSet.Array());

		for (const auto& SwitchRef : RequiredGroupValueSet)
		{
			FWwiseGroupValueCookedData SwitchCookedData;
			switch (SwitchRef.GetType())
			{
			case EWwiseRefType::Switch: SwitchCookedData.Type = EWwiseGroupType::Switch; break;
			case EWwiseRefType::State: SwitchCookedData.Type = EWwiseGroupType::State; break;
			default: SwitchCookedData.Type = EWwiseGroupType::Unknown;
			}
			SwitchCookedData.GroupId = SwitchRef.GetGroupId();
			SwitchCookedData.Id = SwitchRef.GetId();
			if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
			{
				SwitchCookedData.DebugName.Empty();
			}
			else
			{
				SwitchCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? SwitchRef.GetName() : SwitchRef.GetObjectPath();
			}

			CookedData.RequiredGroupValueSet.Add(MoveTemp(SwitchCookedData));
		}

		CookedData.DestroyOptions = InInfo.DestroyOptions;

		OutCookedData.EventLanguageMap.Add(FWwiseLanguageCookedData(LanguageId.GetLanguageId(), LanguageId.GetLanguageName(), LanguageId.LanguageRequirement), MoveTemp(CookedData));
	}

	if (UNLIKELY(OutCookedData.EventLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetEventCookedData (%s %" PRIu32 " %s): No Event"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	// Make this a SFX if all CookedData are identical
	{
		auto& Map = OutCookedData.EventLanguageMap;
		TArray<FWwiseLanguageCookedData> Keys;
		Map.GetKeys(Keys);

		auto LhsKey = Keys.Pop(false);
		const auto* Lhs = Map.Find(LhsKey);
		while (Keys.Num() > 0)
		{
			auto RhsKey = Keys.Pop(false);
			const auto* Rhs = Map.Find(RhsKey);

			if (Lhs->EventId != Rhs->EventId
				|| Lhs->DebugName != Rhs->DebugName
				|| Lhs->SoundBanks.Num() != Rhs->SoundBanks.Num()
				|| Lhs->ExternalSources.Num() != Rhs->ExternalSources.Num()
				|| Lhs->Media.Num() != Rhs->Media.Num()
				|| Lhs->RequiredGroupValueSet.Num() != Rhs->RequiredGroupValueSet.Num()
				|| Lhs->SwitchContainerLeaves.Num() != Rhs->SwitchContainerLeaves.Num())
			{
				UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return true;
			}
			for (const auto& Elem : Lhs->SoundBanks)
			{
				if (!Rhs->SoundBanks.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to banks"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& Elem : Lhs->ExternalSources)
			{
				if (!Rhs->ExternalSources.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to external sources"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& Elem : Lhs->Media)
			{
				if (!Rhs->Media.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to media"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& Elem : Lhs->RequiredGroupValueSet)
			{
				if (!Rhs->RequiredGroupValueSet.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to required group values"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& LhsLeaf : Lhs->SwitchContainerLeaves)
			{
				const auto RhsLeafIndex = Rhs->SwitchContainerLeaves.Find(LhsLeaf);
				if (RhsLeafIndex == INDEX_NONE)
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to switch container"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
				const auto& RhsLeaf = Rhs->SwitchContainerLeaves[RhsLeafIndex];

				for (const auto& Elem : LhsLeaf.SoundBanks)
				{
					if (!RhsLeaf.SoundBanks.Contains(Elem))
					{
						UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to banks in switch container"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return true;
					}
				}
				for (const auto& Elem : LhsLeaf.ExternalSources)
				{
					if (!RhsLeaf.ExternalSources.Contains(Elem))
					{
						UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to external sources in switch container"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return true;
					}
				}
				for (const auto& Elem : LhsLeaf.Media)
				{
					if (!RhsLeaf.Media.Contains(Elem))
					{
						UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event has languages due to media in switch container"),
							*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
						return true;
					}
				}
			}
		}

		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetEventCookedData (%s %" PRIu32 " %s): Event is a SFX"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		std::remove_reference_t<decltype(Map)> SfxMap;
		SfxMap.Add(FWwiseLanguageCookedData::Sfx, *Lhs);

		Map = SfxMap;
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetExternalSourceCookedData(FWwiseExternalSourceCookedData& OutCookedData, uint32 InCookie) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetExternalSourceCookedData (%" PRIu32 "): ProjectDatabase not initialized"),
			InCookie);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetExternalSourceCookedData (%" PRIu32 "): No data for platform"),
			InCookie);
		return false;
	}

	const auto LocalizableId = FWwiseDatabaseLocalizableIdKey(InCookie, FWwiseDatabaseLocalizableIdKey::GENERIC_LANGUAGE);
	const auto* ExternalSourceRef = PlatformData->ExternalSources.Find(LocalizableId);
	if (UNLIKELY(!ExternalSourceRef))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetExternalSourceCookedData (%" PRIu32 "): Could not find External Source"),
			InCookie);
		return false;
	}
	const auto* ExternalSource = ExternalSourceRef->GetExternalSource();
	if (UNLIKELY(!ExternalSource))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetExternalSourceCookedData (%" PRIu32 "): Could not get External Source"),
			InCookie);
		return false;
	}

	if (UNLIKELY(!FillExternalSourceBaseInfo(OutCookedData, *ExternalSource)))
	{
		return false;
	}
	return true;
}

bool UWwiseResourceCookerImpl::GetGameParameterCookedData(FWwiseGameParameterCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetGameParameterCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetGameParameterCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	FWwiseRefGameParameter GameParameterRef;

	if (UNLIKELY(!PlatformData->GetRef(GameParameterRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetGameParameterCookedData (%s %" PRIu32 " %s): No game parameter found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const auto* GameParameter = GameParameterRef.GetGameParameter();

	OutCookedData.ShortId = GameParameter->Id;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutCookedData.DebugName.Empty();
	}
	else
	{
		OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? GameParameter->Name : GameParameter->ObjectPath;
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetInitBankCookedData(FWwiseInitBankCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const auto* PlatformInfo = PlatformData->PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): No Platform Info"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	FWwiseRefSoundBank SoundBankRef;
	if (UNLIKELY(!PlatformData->GetRef(SoundBankRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): No ref found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	if (UNLIKELY(!SoundBankRef.IsInitBank()))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): Not an init SoundBank"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	TSet<FWwiseSoundBankCookedData> AdditionalSoundBanks;
	{
		const auto* SoundBank = SoundBankRef.GetSoundBank();
		if (UNLIKELY(!SoundBank))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}
		if (UNLIKELY(!FillSoundBankBaseInfo(OutCookedData, *PlatformInfo, *SoundBank)))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetInitBankCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}

		// Add all Init SoundBank media
		TSet<FWwiseMediaCookedData> MediaSet;
		{
			for (const auto& MediaRef : SoundBankRef.GetMedia(PlatformData->MediaFiles))
			{
				if (UNLIKELY(!AddRequirementsForMedia(AdditionalSoundBanks, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
				{
					return false;
				}
			}
		}

		{
			WwiseCustomPluginIdsMap CustomPluginsRefs = SoundBankRef.GetCustomPlugins(PlatformData->CustomPlugins);
			for (const auto& Plugin : CustomPluginsRefs)
			{
				const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(AdditionalSoundBanks, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}

		{
			WwisePluginSharesetIdsMap ShareSetRefs = SoundBankRef.GetPluginSharesets(PlatformData->PluginSharesets);
			for (const auto& ShareSet : ShareSetRefs)
			{
				const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(AdditionalSoundBanks, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}

		{
			WwiseAudioDeviceIdsMap AudioDevicesRefs = SoundBankRef.GetAudioDevices(PlatformData->AudioDevices);
			for (const auto& AudioDevice : AudioDevicesRefs)
			{
				const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(AdditionalSoundBanks, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}


		OutCookedData.Media = MediaSet.Array();
		const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();
		OutCookedData.Language.Empty(Languages.Num());
		for (const FWwiseSharedLanguageId& Language : Languages)
		{
			OutCookedData.Language.Add({ Language.GetLanguageId(), Language.GetLanguageName(), Language.LanguageRequirement });
		}

		if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
		{
			OutCookedData.DebugName.Empty();
		}
		else
		{
			OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? SoundBank->ShortName : SoundBank->ObjectPath;
		}
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetMediaCookedData(FWwiseMediaCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): ProjectDatabase not initialized"),
			InInfo.AssetShortId);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): No data for platform"),
			InInfo.AssetShortId);
		return false;
	}

	auto MediaId = FWwiseDatabaseMediaIdKey(InInfo.AssetShortId, InInfo.HardCodedSoundBankShortId);
	const auto* MediaRef = PlatformData->MediaFiles.Find(MediaId);
	if (UNLIKELY(!MediaRef))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): Could not find Media in SoundBank %" PRIu32),
			InInfo.AssetShortId, InInfo.HardCodedSoundBankShortId);
		return false;
	}

	const FWwiseSharedLanguageId* LanguageRefPtr = nullptr;
	if (MediaRef->LanguageId)
	{
		const auto& Languages = DataStructure.GetLanguages();
		LanguageRefPtr = Languages.Find(FWwiseSharedLanguageId(MediaRef->LanguageId, TEXT(""), EWwiseLanguageRequirement::IsOptional));
		if (UNLIKELY(!LanguageRefPtr))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): Could not find language %" PRIu32),
				InInfo.AssetShortId, MediaRef->LanguageId);
			return false;
		}
	}
	const auto& LanguageRef = LanguageRefPtr ? *LanguageRefPtr : FWwiseSharedLanguageId();

	TSet<FWwiseSoundBankCookedData> SoundBankSet;
	TSet<FWwiseMediaCookedData> MediaSet;
	if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, *MediaRef, LanguageRef, *PlatformData)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): Could not get requirements for media."),
			InInfo.AssetShortId);
		return false;
	}

	if (UNLIKELY(SoundBankSet.Num() > 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetMediaCookedData (%" PRIu32 "): Asking for a media in a particular SoundBank (%" PRIu32 ") must have it fully defined in this SoundBank."),
			InInfo.AssetShortId, InInfo.HardCodedSoundBankShortId);
		return false;
	}

	if (MediaSet.Num() == 0)
	{
		// Not directly an error: Media is in this SoundBank, without streaming. Can be a logical error, but it's not our error.
		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetMediaCookedData (%" PRIu32 "): Media is fully in SoundBank. Returning no media."),
			InInfo.AssetShortId);
		return false;
	}

	auto Media = MediaSet.Array()[0];

	OutCookedData = MoveTemp(Media);
	return true;
}

bool UWwiseResourceCookerImpl::GetSharesetCookedData(FWwiseLocalizedSharesetCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();
				
	const auto* PlatformInfo = PlatformData->PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): No Platform Info"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	TMap<FWwiseSharedLanguageId, FWwiseRefPluginShareset> RefLanguageMap;
	PlatformData->GetRefMap(RefLanguageMap, Languages, InInfo);
	if (UNLIKELY(RefLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): No ref found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.SharesetLanguageMap.Empty(RefLanguageMap.Num());

	for (const auto& Ref : RefLanguageMap)
	{
		FWwiseSharesetCookedData CookedData;
		const auto* Shareset = Ref.Value.GetPlugin();
		if (UNLIKELY(!Shareset))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Could not get Shareset from Ref"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}
		CookedData.SharesetId = Shareset->Id;

		const auto* SoundBank = Ref.Value.GetSoundBank();
		if (UNLIKELY(!SoundBank))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}
		TSet<FWwiseSoundBankCookedData> SoundBankSet;
		if (!SoundBank->IsInitBank())
		{
			FWwiseSoundBankCookedData MainSoundBank;
			if (UNLIKELY(!FillSoundBankBaseInfo(MainSoundBank, *PlatformInfo, *SoundBank)))
			{
				UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return false;
			}
			SoundBankSet.Add(MainSoundBank);
		}

		TSet<FWwiseMediaCookedData> MediaSet;
		{
			WwiseMediaIdsMap MediaRefs = Ref.Value.GetMedia(PlatformData->MediaFiles);
			for (const auto& MediaRef : MediaRefs)
			{
				if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, Ref.Key, *PlatformData)))
				{
					UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Could not fill Media from Data"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return false;
				}
			}
		}

		{
			WwiseCustomPluginIdsMap CustomPluginsRefs = Ref.Value.GetCustomPlugins(PlatformData->CustomPlugins);
			for (const auto& Plugin : CustomPluginsRefs)
			{
				const WwiseMediaIdsMap MediaRefs = Plugin.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}

		{
			WwisePluginSharesetIdsMap ShareSetRefs = Ref.Value.GetPluginSharesets(PlatformData->PluginSharesets);
			for (const auto& ShareSet : ShareSetRefs)
			{
				const WwiseMediaIdsMap MediaRefs = ShareSet.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}

		{
			WwiseAudioDeviceIdsMap AudioDevicesRefs = Ref.Value.GetAudioDevices(PlatformData->AudioDevices);
			for (const auto& AudioDevice : AudioDevicesRefs)
			{
				const WwiseMediaIdsMap MediaRefs = AudioDevice.Value.GetMedia(PlatformData->MediaFiles);
				for (const auto& MediaRef : MediaRefs)
				{
					if (UNLIKELY(!AddRequirementsForMedia(SoundBankSet, MediaSet, MediaRef.Value, FWwiseSharedLanguageId(), *PlatformData)))
					{
						return false;
					}
				}
			}
		}

		CookedData.SoundBanks = SoundBankSet.Array();
		CookedData.Media = MediaSet.Array();

		if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
		{
			OutCookedData.DebugName.Empty();
		}
		else
		{
			CookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? Shareset->Name : Shareset->ObjectPath;
			OutCookedData.DebugName = CookedData.DebugName;
		}

		OutCookedData.SharesetId = CookedData.SharesetId;
		OutCookedData.SharesetLanguageMap.Add(FWwiseLanguageCookedData(Ref.Key.GetLanguageId(), Ref.Key.GetLanguageName(), Ref.Key.LanguageRequirement), MoveTemp(CookedData));
	}

	if (UNLIKELY(OutCookedData.SharesetLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): No Shareset"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	// Make this a SFX if all CookedData are identical
	{
		auto& Map = OutCookedData.SharesetLanguageMap;
		TArray<FWwiseLanguageCookedData> Keys;
		Map.GetKeys(Keys);

		auto LhsKey = Keys.Pop(false);
		const auto* Lhs = Map.Find(LhsKey);
		while (Keys.Num() > 0)
		{
			auto RhsKey = Keys.Pop(false);
			const auto* Rhs = Map.Find(RhsKey);

			if (Lhs->SharesetId != Rhs->SharesetId
				|| Lhs->DebugName != Rhs->DebugName
				|| Lhs->SoundBanks.Num() != Rhs->SoundBanks.Num()
				|| Lhs->Media.Num() != Rhs->Media.Num())
			{
				UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Shareset has languages"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return true;
			}
			for (const auto& Elem : Lhs->SoundBanks)
			{
				if (!Rhs->SoundBanks.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Shareset has languages due to banks"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
			for (const auto& Elem : Lhs->Media)
			{
				if (!Rhs->Media.Contains(Elem))
				{
					UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Shareset has languages due to media"),
						*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
					return true;
				}
			}
		}

		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSharesetCookedData (%s %" PRIu32 " %s): Shareset is a SFX"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		std::remove_reference_t<decltype(Map)> SfxMap;
		SfxMap.Add(FWwiseLanguageCookedData::Sfx, *Lhs);

		Map = SfxMap;
	}

	return true;
}

bool UWwiseResourceCookerImpl::GetSoundBankCookedData(FWwiseLocalizedSoundBankCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();

	const auto* PlatformInfo = PlatformData->PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): No Platform Info"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}
	
	TMap<FWwiseSharedLanguageId, FWwiseRefSoundBank> RefLanguageMap;
	PlatformData->GetRefMap(RefLanguageMap, Languages, InInfo);
	if (UNLIKELY(RefLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): No ref found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.SoundBankLanguageMap.Empty(RefLanguageMap.Num());

	for (const auto& Ref : RefLanguageMap)
	{
		FWwiseSoundBankCookedData CookedData;
		const auto* SoundBank = Ref.Value.GetSoundBank();
		if (UNLIKELY(!SoundBank))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): Could not get SoundBank from Ref"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}
		if (UNLIKELY(!FillSoundBankBaseInfo(CookedData, *PlatformInfo, *SoundBank)))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): Could not fill SoundBank from Data"),
				*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
			return false;
		}

		if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
		{
			OutCookedData.DebugName.Empty();
		}
		else
		{
			CookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? SoundBank->ShortName : SoundBank->ObjectPath;
			OutCookedData.DebugName = CookedData.DebugName;
		}

		OutCookedData.SoundBankId = CookedData.SoundBankId;
		OutCookedData.SoundBankLanguageMap.Add(FWwiseLanguageCookedData(Ref.Key.GetLanguageId(), Ref.Key.GetLanguageName(), Ref.Key.LanguageRequirement), MoveTemp(CookedData));
	}

	if (UNLIKELY(OutCookedData.SoundBankLanguageMap.Num() == 0))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): No SoundBank"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	// Make this a SFX if all CookedData are identical
	{
		auto& Map = OutCookedData.SoundBankLanguageMap;
		TArray<FWwiseLanguageCookedData> Keys;
		Map.GetKeys(Keys);

		auto LhsKey = Keys.Pop(false);
		const auto* Lhs = Map.Find(LhsKey);
		while (Keys.Num() > 0)
		{
			auto RhsKey = Keys.Pop(false);
			const auto* Rhs = Map.Find(RhsKey);

			if (GetTypeHash(*Lhs) != GetTypeHash(*Rhs))
			{
				UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): SoundBank has languages"),
					*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
				return true;
			}
		}

		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("GetSoundBankCookedData (%s %" PRIu32 " %s): SoundBank is a SFX"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		std::remove_reference_t<decltype(Map)> SfxMap;
		SfxMap.Add(FWwiseLanguageCookedData::Sfx, *Lhs);

		Map = SfxMap;
	}
	return true;
}

bool UWwiseResourceCookerImpl::GetStateCookedData(FWwiseGroupValueCookedData& OutCookedData, const FWwiseGroupValueInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetStateCookedData (%s %" PRIu32 " %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetStateCookedData (%s %" PRIu32 " %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	FWwiseRefState StateRef;
	if (UNLIKELY(!PlatformData->GetRef(StateRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetStateCookedData (%s %" PRIu32 " %" PRIu32 " %s): No state found"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}
	const auto* State = StateRef.GetState();
	const auto* StateGroup = StateRef.GetStateGroup();
	if (UNLIKELY(!State || !StateGroup))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetStateCookedData (%s %" PRIu32 " %" PRIu32 " %s): No state in ref"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.Type = EWwiseGroupType::State;
	OutCookedData.GroupId = StateGroup->Id;
	OutCookedData.Id = State->Id;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutCookedData.DebugName.Empty();
	}
	else
	{
		OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? State->Name : State->ObjectPath;
	}
	return true;
}

bool UWwiseResourceCookerImpl::GetSwitchCookedData(FWwiseGroupValueCookedData& OutCookedData, const FWwiseGroupValueInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSwitchCookedData (%s %" PRIu32 " %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSwitchCookedData (%s %" PRIu32 " %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	FWwiseRefSwitch SwitchRef;
	if (UNLIKELY(!PlatformData->GetRef(SwitchRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSwitchCookedData (%s %" PRIu32 " %" PRIu32 " %s): No switch found"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}
	const auto* Switch = SwitchRef.GetSwitch();
	const auto* SwitchGroup = SwitchRef.GetSwitchGroup();
	if (UNLIKELY(!Switch || !SwitchGroup))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetSwitchCookedData (%s %" PRIu32 " %" PRIu32 " %s): No switch in ref"),
			*InInfo.AssetGuid.ToString(), InInfo.GroupShortId, InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	OutCookedData.Type = EWwiseGroupType::Switch;
	OutCookedData.GroupId = SwitchGroup->Id;
	OutCookedData.Id = Switch->Id;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutCookedData.DebugName.Empty();
	}
	else
	{
		OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? Switch->Name : Switch->ObjectPath;
	}
	return true;
}

bool UWwiseResourceCookerImpl::GetTriggerCookedData(FWwiseTriggerCookedData& OutCookedData, const FWwiseAssetInfo& InInfo) const
{
	const auto* ProjectDatabase = GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetTriggerCookedData (%s %" PRIu32 " %s): ProjectDatabase not initialized"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto* PlatformData = DataStructure.GetCurrentPlatformData();
	if (UNLIKELY(!PlatformData))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetTriggerCookedData (%s %" PRIu32 " %s): No data for platform"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const TSet<FWwiseSharedLanguageId>& Languages = DataStructure.GetLanguages();

	FWwiseRefTrigger TriggerRef;

	if (UNLIKELY(!PlatformData->GetRef(TriggerRef, FWwiseSharedLanguageId(), InInfo)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("GetTriggerCookedData (%s %" PRIu32 " %s): No trigger data found"),
			*InInfo.AssetGuid.ToString(), InInfo.AssetShortId, *InInfo.AssetName);
		return false;
	}

	const auto* Trigger = TriggerRef.GetTrigger();

	OutCookedData.TriggerId = Trigger->Id;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutCookedData.DebugName.Empty();
	}
	else
	{
		OutCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? Trigger->Name : Trigger->ObjectPath;
	}

	return true;
}

bool UWwiseResourceCookerImpl::FillSoundBankBaseInfo(FWwiseSoundBankCookedData& OutSoundBankCookedData, const FWwiseMetadataPlatformInfo& InPlatformInfo, const FWwiseMetadataSoundBank& InSoundBank) const
{
	OutSoundBankCookedData.SoundBankId = InSoundBank.Id;
	OutSoundBankCookedData.SoundBankPathName = InSoundBank.Path;
	OutSoundBankCookedData.MemoryAlignment = InSoundBank.Align == 0 ? InPlatformInfo.DefaultAlign : InSoundBank.Align;
	OutSoundBankCookedData.bDeviceMemory = InSoundBank.bDeviceMemory;
	OutSoundBankCookedData.bContainsMedia = InSoundBank.ContainsMedia();
	switch (InSoundBank.Type)
	{
	case EMetadataSoundBankType::Bus:
		OutSoundBankCookedData.SoundBankType = EWwiseSoundBankType::Bus;
		break;
	case EMetadataSoundBankType::Event:
		OutSoundBankCookedData.SoundBankType = EWwiseSoundBankType::Event;
		break;
	case EMetadataSoundBankType::User:
	default:
		OutSoundBankCookedData.SoundBankType = EWwiseSoundBankType::User;
	}
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutSoundBankCookedData.DebugName.Empty();
	}
	else
	{
		OutSoundBankCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? InSoundBank.ShortName : InSoundBank.ObjectPath;
	}
	return true;
}

bool UWwiseResourceCookerImpl::FillMediaBaseInfo(FWwiseMediaCookedData& OutMediaCookedData, const FWwiseMetadataPlatformInfo& InPlatformInfo, const FWwiseMetadataSoundBank& InSoundBank, const FWwiseMetadataMediaReference& InMediaReference) const
{
	for (const auto& Media : InSoundBank.Media)
	{
		if (Media.Id == InMediaReference.Id)
		{
			return FillMediaBaseInfo(OutMediaCookedData, InPlatformInfo, InSoundBank, Media);
		}
	}
	UE_LOG(LogWwiseResourceCooker, Error, TEXT("FillMediaBaseInfo: Could not get Media Reference %" PRIu32 " in SoundBank %s %" PRIu32),
		InMediaReference.Id, *InSoundBank.ShortName, InSoundBank.Id);
	return false;
}

bool UWwiseResourceCookerImpl::FillMediaBaseInfo(FWwiseMediaCookedData& OutMediaCookedData, const FWwiseMetadataPlatformInfo& InPlatformInfo, const FWwiseMetadataSoundBank& InSoundBank, const FWwiseMetadataMedia& InMedia) const
{
	OutMediaCookedData.MediaId = InMedia.Id;
	if (InMedia.Path.IsEmpty())
	{
		if (UNLIKELY(InMedia.CachePath.IsEmpty()))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("FillMediaBaseInfo: Empty path for Media %" PRIu32 " in SoundBank %s %" PRIu32),
				InMedia.Id, *InSoundBank.ShortName, InSoundBank.Id);
			return false;
		}
		OutMediaCookedData.MediaPathName = InMedia.CachePath;
	}
	else
	{
		OutMediaCookedData.MediaPathName = InMedia.Path;
	}
	OutMediaCookedData.PrefetchSize = InMedia.PrefetchSize;
	OutMediaCookedData.MemoryAlignment = InMedia.Align == 0 ? InPlatformInfo.DefaultAlign : InMedia.Align;
	OutMediaCookedData.bDeviceMemory = InMedia.bDeviceMemory;
	OutMediaCookedData.bStreaming = InMedia.bStreaming;

	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutMediaCookedData.DebugName.Empty();
	}
	else
	{
		OutMediaCookedData.DebugName = InMedia.ShortName;
	}
	return true;
}

bool UWwiseResourceCookerImpl::FillExternalSourceBaseInfo(FWwiseExternalSourceCookedData& OutExternalSourceCookedData, const FWwiseMetadataExternalSource& InExternalSource) const
{
	OutExternalSourceCookedData.Cookie = InExternalSource.Cookie;
	if (ExportDebugNameRule == EWwiseExportDebugNameRule::Release)
	{
		OutExternalSourceCookedData.DebugName.Empty();
	}
	else
	{
		OutExternalSourceCookedData.DebugName = (ExportDebugNameRule == EWwiseExportDebugNameRule::Name) ? InExternalSource.Name : InExternalSource.ObjectPath;
	}
	return true;
}

bool UWwiseResourceCookerImpl::AddRequirementsForMedia(TSet<FWwiseSoundBankCookedData>& OutSoundBankSet, TSet<FWwiseMediaCookedData>& OutMediaSet,
	const FWwiseRefMedia& InMediaRef, const FWwiseSharedLanguageId& InLanguage,
	const FWwisePlatformDataStructure& InPlatformData) const
{
	const auto* Media = InMediaRef.GetMedia();
	if (UNLIKELY(!Media))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia: Could not get Media from Media Ref"));
		return false;
	}

	const auto* PlatformInfo = InPlatformData.PlatformRef.GetPlatformInfo();
	if (UNLIKELY(!PlatformInfo)) return false;

	if (Media->Location == EWwiseMetadataMediaLocation::Memory && !Media->bStreaming)
	{
		// In-Memory media is already loaded with current SoundBank
		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Media is in memory and not streaming. Skipping."),
			*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
	}
	else if (Media->Location == EWwiseMetadataMediaLocation::OtherBank)
	{
		// Media resides in another SoundBank. Find that other SoundBank and add it as a requirement.
		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Media is in another SoundBank. Locate SoundBank and add requirement."),
			*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());

		FWwiseAssetInfo MediaInfo;
		MediaInfo.AssetShortId = Media->Id;
		MediaInfo.AssetName = Media->ShortName;

		FWwiseRefMedia OtherSoundBankMediaRef;
		if (UNLIKELY(!InPlatformData.GetRef(OtherSoundBankMediaRef, InLanguage, MediaInfo)))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Could not get Ref for other SoundBank media %s %" PRIu32 " %s"),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId(),
				*MediaInfo.AssetGuid.ToString(), MediaInfo.AssetShortId, *MediaInfo.AssetName);
			return false;
		}

		const auto* SoundBank = OtherSoundBankMediaRef.GetSoundBank();
		if (UNLIKELY(!SoundBank))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Could not get SoundBank from Media in another SoundBank Ref"),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
			return false;
		}

		if (SoundBank->IsInitBank())
		{
			// We assume Init SoundBanks are fully loaded
			UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Media is in Init SoundBank. Skipping."),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
		}

		FWwiseSoundBankCookedData MediaSoundBank;
		if (UNLIKELY(!FillSoundBankBaseInfo(MediaSoundBank, *PlatformInfo, *SoundBank)))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Could not fill SoundBank from Media in another SoundBank Data"),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
			return false;
		}
		OutSoundBankSet.Add(MoveTemp(MediaSoundBank));
	}
	else
	{
		// Media has a required loose file.
		UE_LOG(LogWwiseResourceCooker, VeryVerbose, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Adding loose media requirement."),
			*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());

		const auto* SoundBank = InMediaRef.GetSoundBank();
		if (UNLIKELY(!SoundBank))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Could not get SoundBank from Media"),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
			return false;
		}

		FWwiseMediaCookedData MediaCookedData;
		if (UNLIKELY(!FillMediaBaseInfo(MediaCookedData, *PlatformInfo, *SoundBank, *Media)))
		{
			UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForMedia (%s %" PRIu32 " in %s %" PRIu32 "): Could not fill Media from Media Ref"),
				*Media->ShortName, Media->Id, *InLanguage.GetLanguageName(), InLanguage.GetLanguageId());
			return false;
		}

		OutMediaSet.Add(MoveTemp(MediaCookedData));
	}

	return true;
}

bool UWwiseResourceCookerImpl::AddRequirementsForExternalSource(TSet<FWwiseExternalSourceCookedData>& OutExternalSourceSet, const FWwiseRefExternalSource& InExternalSourceRef) const
{
	const auto* ExternalSource = InExternalSourceRef.GetExternalSource();
	if (UNLIKELY(!ExternalSource))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForExternalSource: Could not get External Source from External Source Ref"));
		return false;
	}
	FWwiseExternalSourceCookedData ExternalSourceCookedData;
	if (UNLIKELY(!FillExternalSourceBaseInfo(ExternalSourceCookedData, *ExternalSource)))
	{
		UE_LOG(LogWwiseResourceCooker, Error, TEXT("AddRequirementsForExternalSource (%s %" PRIu32 "): Could not fill External Source from External Source Ref"),
			*ExternalSource->Name, ExternalSource->Cookie);
		return false;
	}
	OutExternalSourceSet.Add(MoveTemp(ExternalSourceCookedData));
	return true;
}
