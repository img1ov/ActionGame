// Fill out your copyright notice in the Description page of Project Settings.


#include "System/ActGameData.h"

#include "System/ActAssetManager.h"

UActGameData::UActGameData()
{
}

const UActGameData& UActGameData::Get()
{
	return UActAssetManager::Get().GetGameData();
}
