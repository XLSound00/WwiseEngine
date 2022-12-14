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

#include "Wwise/CookedData/WwiseSwitchContainerLeafCookedData.h"

FWwiseSwitchContainerLeafCookedData::FWwiseSwitchContainerLeafCookedData():
	GroupValueSet(),
	SoundBanks(),
	Media(),
	ExternalSources()
{}

void FWwiseSwitchContainerLeafCookedData::Serialize(FArchive& Ar)
{
	UStruct* Struct = StaticStruct();
	check(Struct);

	if (Ar.WantBinaryPropertySerialization())
	{
		Struct->SerializeBin(Ar, this);
	}
	else
	{
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}
}

bool FWwiseSwitchContainerLeafCookedData::operator==(const FWwiseSwitchContainerLeafCookedData& Rhs) const
{
	if (GroupValueSet.Num() != Rhs.GroupValueSet.Num())
	{
		return false;
	}
	for (const auto& Elem : GroupValueSet)
	{
		if (!Rhs.GroupValueSet.Contains(Elem))
		{
			return false;
		}
	}
	return true;
}
