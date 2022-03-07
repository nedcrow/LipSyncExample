// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OVRLipSyncFrame.h"
#include "AzureTTSComponent.generated.h"

/*
* REST API는 Visme 기능을 지원하지 않음 https://techcommunity.microsoft.com/t5/ai-cognitive-services-blog/azure-neural-text-to-speech-extended-to-support-lip-sync-with/ba-p/2356748
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AZURETTSDEMO_API UAzureTTSComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAzureTTSComponent();

/* Properties */
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (Tooltip = "Use when functions(OVRLipSyncProcess) has null sound property."))
	USoundBase* DefaultSoundBase;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Meta = (Tooltip = "LipSync Sequence to be played"))
	UOVRLipSyncFrameSequence* LipSyncSequence;

public:
	UFUNCTION(BlueprintCallable)
	void StartLipSync();

/*
* OVRLipSyncProcess
* 매개변수로 LipSyncSequence 생성
* Reference : OVRLipSyncEditorModule
*/
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundWave* LastSoundWave;

	UFUNCTION(BlueprintCallable)
	bool OVRLipSyncProcessSoundWaveAsset(const FAssetData& SoundWaveAsset, bool UseOfflineModel = false);
	UFUNCTION(BlueprintCallable)
	bool OVRLipSyncProcessSoundBase(USoundBase* TargetSoundBase, bool UseOfflineModel = false);
	UFUNCTION(BlueprintCallable)
	bool OVRLipSyncProcessSoundWave(USoundWave* TargetSoundWave, bool UseOfflineModel = false);

	UFUNCTION(BlueprintCallable)
	bool ConvertSoundData(TArray<uint8> Data);

/* Just Private */
private:
	
	bool DecompressSoundWave(USoundWave* SoundWave);
	bool IsStop(FString LogStr);
};
