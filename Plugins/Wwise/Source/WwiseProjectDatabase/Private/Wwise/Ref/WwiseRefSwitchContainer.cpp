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

#include "Wwise/Ref/WwiseRefSwitchContainer.h"

#include "Wwise/Stats/ProjectDatabase.h"
#include "Wwise/Ref/WwiseAnyRef.h"
#include "Wwise/Ref/WwiseRefAudioDevice.h"
#include "Wwise/Ref/WwiseRefCustomPlugin.h"
#include "Wwise/Ref/WwiseRefExternalSource.h"
#include "Wwise/Ref/WwiseRefMedia.h"
#include "Wwise/Ref/WwiseRefPluginShareset.h"
#include "Wwise/Ref/WwiseRefState.h"
#include "Wwise/WwiseProjectDatabaseModule.h"
#include "Wwise/Ref/WwiseRefSwitch.h"
#include "Wwise/Metadata/WwiseMetadataEvent.h"
#include "Wwise/Metadata/WwiseMetadataMedia.h"
#include "Wwise/Metadata/WwiseMetadataPlugin.h"
#include "Wwise/Metadata/WwiseMetadataPluginGroup.h"
#include "Wwise/Metadata/WwiseMetadataSoundBank.h"
#include "Wwise/Metadata/WwiseMetadataSwitchContainer.h"
#include "Wwise/Metadata/WwiseMetadataSwitchValue.h"

const TCHAR* const FWwiseRefSwitchContainer::NAME = TEXT("SwitchContainer");

const FWwiseMetadataSwitchContainer* FWwiseRefSwitchContainer::GetSwitchContainer() const
{
	const auto* Event = GetEvent();
	if (UNLIKELY(!Event))
	{
		return nullptr;
	}

	const auto* SwitchContainers = &Event->SwitchContainers;
	const FWwiseMetadataSwitchContainer* Result = nullptr;
	for (auto Index : ChildrenIndices)
	{
		if (!SwitchContainers->IsValidIndex(Index))
		{
			UE_LOG(LogWwiseProjectDatabase, Error, TEXT("Could not get Switch Container index #%zu"), Index);
			return nullptr;
		}
		Result = &(*SwitchContainers)[Index];
		SwitchContainers = &Result->Children;
	}
	return Result;
}

FWwiseAnyRef FWwiseRefSwitchContainer::GetSwitchValue(const WwiseSwitchGlobalIdsMap& SwitchGlobalMap, const WwiseStateGlobalIdsMap& StateGlobalMap) const
{
	const auto* Container = GetSwitchContainer();
	if (!Container)
	{
		return {};
	}
	const auto& SwitchValue = Container->SwitchValue;
	switch (SwitchValue.GroupType)
	{
	case EWwiseMetadataSwitchValueGroupType::Switch:
	{
		const auto* GlobalRef = SwitchGlobalMap.Find(FWwiseDatabaseLocalizableGroupValueKey(SwitchValue.GroupId, SwitchValue.Id, LanguageId));
		if (UNLIKELY(!GlobalRef))
		{
			return {};
		}
		return FWwiseAnyRef::Create(*GlobalRef);
	}
	case EWwiseMetadataSwitchValueGroupType::State:
	{
		const auto* GlobalRef = StateGlobalMap.Find(FWwiseDatabaseLocalizableGroupValueKey(SwitchValue.GroupId, SwitchValue.Id, LanguageId));
		if (UNLIKELY(!GlobalRef))
		{
			return {};
		}
		return FWwiseAnyRef::Create(*GlobalRef);
	}
	}
	return {};
}

WwiseMediaIdsMap FWwiseRefSwitchContainer::GetMedia(const WwiseMediaGlobalIdsMap& GlobalMap) const
{
	const auto* SwitchContainer = GetSwitchContainer();
	const auto* SoundBank = GetSoundBank();
	TArray<FWwiseDatabaseMediaIdKey> MapKeys;
	if (!SwitchContainer || !SoundBank)
	{
		return {};
	}
	const auto& MediaRefs = SwitchContainer->MediaRefs;
	WwiseMediaIdsMap Result;
	Result.Empty(MediaRefs.Num());
	for (const auto& Elem : MediaRefs)
	{
		FWwiseDatabaseMediaIdKey SoundBankFileId(Elem.Id, SoundBank->Id);
		const auto* GlobalRef = GlobalMap.Find(SoundBankFileId);
		if (GlobalRef)
		{
			Result.Add(Elem.Id, *GlobalRef);
		}
	}
	return Result;
}

WwiseExternalSourceIdsMap FWwiseRefSwitchContainer::GetExternalSources(const WwiseExternalSourceGlobalIdsMap& GlobalMap) const
{
	const auto* SwitchContainer = GetSwitchContainer();
	if (!SwitchContainer)
	{
		return {};
	}
	const auto& ExternalSourceRefs = SwitchContainer->ExternalSourceRefs;
	WwiseExternalSourceIdsMap Result;
	Result.Empty(ExternalSourceRefs.Num());
	for (const auto& Elem : ExternalSourceRefs)
	{
		FWwiseDatabaseLocalizableIdKey Id(Elem.Cookie, LanguageId);
		const auto* GlobalRef = GlobalMap.Find(Id);
		if (GlobalRef)
		{
			Result.Add(Elem.Cookie, *GlobalRef);
		}
	}
	return Result;
}

WwiseCustomPluginIdsMap FWwiseRefSwitchContainer::GetCustomPlugins(const WwiseCustomPluginGlobalIdsMap& GlobalMap) const
{
	const auto* SwitchContainer = GetSwitchContainer();
	if (!SwitchContainer || !SwitchContainer->PluginRefs)
	{
		return {};
	}
	const auto& Plugins = SwitchContainer->PluginRefs->Custom;
	WwiseCustomPluginIdsMap Result;
	Result.Empty(Plugins.Num());
	for (const auto& Elem : Plugins)
	{
		FWwiseDatabaseLocalizableIdKey Id(Elem.Id, LanguageId);
		const auto* GlobalRef = GlobalMap.Find(Id);
		if (GlobalRef)
		{
			Result.Add(Elem.Id, *GlobalRef);
		}
	}
	return Result;
}

WwisePluginSharesetIdsMap FWwiseRefSwitchContainer::GetPluginSharesets(const WwisePluginSharesetGlobalIdsMap& GlobalMap) const
{
	const auto* SwitchContainer = GetSwitchContainer();
	if (!SwitchContainer || !SwitchContainer->PluginRefs)
	{
		return {};
	}
	const auto& Plugins = SwitchContainer->PluginRefs->Sharesets;
	WwisePluginSharesetIdsMap Result;
	Result.Empty(Plugins.Num());
	for (const auto& Elem : Plugins)
	{
		FWwiseDatabaseLocalizableIdKey Id(Elem.Id, LanguageId);
		const auto* GlobalRef = GlobalMap.Find(Id);
		if (GlobalRef)
		{
			Result.Add(Elem.Id, *GlobalRef);
		}
	}
	return Result;
}

WwiseAudioDeviceIdsMap FWwiseRefSwitchContainer::GetAudioDevices(const WwiseAudioDeviceGlobalIdsMap& GlobalMap) const
{
	const auto* SwitchContainer = GetSwitchContainer();
	if (!SwitchContainer || !SwitchContainer->PluginRefs)
	{
		return {};
	}
	const auto& Plugins = SwitchContainer->PluginRefs->AudioDevices;
	WwiseAudioDeviceIdsMap Result;
	Result.Empty(Plugins.Num());
	for (const auto& Elem : Plugins)
	{
		FWwiseDatabaseLocalizableIdKey Id(Elem.Id, LanguageId);
		const auto* GlobalRef = GlobalMap.Find(Id);
		if (GlobalRef)
		{
			Result.Add(Elem.Id, *GlobalRef);
		}
	}
	return Result;
}

TArray<FWwiseAnyRef> FWwiseRefSwitchContainer::GetSwitchValues(const WwiseSwitchGlobalIdsMap& SwitchGlobalMap, const WwiseStateGlobalIdsMap& StateGlobalMap) const
{
	const auto* Event = GetEvent();
	if (!Event)
	{
		return {};
	}

	const auto* SwitchContainers = &Event->SwitchContainers;
	TArray<FWwiseAnyRef> Result;
	for (auto Index : ChildrenIndices)
	{
		if (UNLIKELY(!SwitchContainers->IsValidIndex(Index)))
		{
			return {};
		}
		const auto& SwitchContainer = (*SwitchContainers)[Index];
		const auto& SwitchValue = SwitchContainer.SwitchValue;

		// Skipping Default Switches, but keep different ones
		if (!SwitchValue.bDefault)
		{
			switch (SwitchValue.GroupType)
			{
			case EWwiseMetadataSwitchValueGroupType::Switch:
			{
				const auto* GlobalRef = SwitchGlobalMap.Find(FWwiseDatabaseLocalizableGroupValueKey(SwitchValue.GroupId, SwitchValue.Id, LanguageId));
				if (UNLIKELY(!GlobalRef))
				{
					return {};
				}
				Result.Add(FWwiseAnyRef::Create(*GlobalRef));
				break;
			}
			case EWwiseMetadataSwitchValueGroupType::State:
			{
				const auto* GlobalRef = StateGlobalMap.Find(FWwiseDatabaseLocalizableGroupValueKey(SwitchValue.GroupId, SwitchValue.Id, LanguageId));
				if (UNLIKELY(!GlobalRef))
				{
					return {};
				}
				Result.Add(FWwiseAnyRef::Create(*GlobalRef));
				break;
			}
			default:
				return {};
			}
		}

		SwitchContainers = &SwitchContainer.Children;
	}
	return Result;
}

uint32 FWwiseRefSwitchContainer::Hash() const
{
	auto Result = FWwiseRefEvent::Hash();
	if (ChildrenIndices.Num() > 0)
	{
		Result = HashCombine(Result, uint32(CityHash64((const char*)ChildrenIndices.GetData(), ChildrenIndices.GetTypeSize() * ChildrenIndices.Num())));
	}
	return Result;
}
