// Fill out your copyright notice in the Description page of Project Settings.


#include "AzureTTSComponent.h"
#include "AsyncTaskAzureTTS.h"
#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine.h"
#include "OVRLipSyncContextWrapper.h"
#include "OVRLipSyncFrame.h"
#include "OVRLipSyncPlaybackActorComponent.h"


// Sets default values for this component's properties
UAzureTTSComponent::UAzureTTSComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UAzureTTSComponent::StartLipSync()
{
	AActor* owner = GetOwner();
	UOVRLipSyncPlaybackActorComponent* OVRLipSyncPlaybackActor = Cast<UOVRLipSyncPlaybackActorComponent>(owner->GetComponentByClass(UOVRLipSyncPlaybackActorComponent().StaticClass()));
	UAudioComponent* audio = Cast<UAudioComponent>(owner->GetComponentByClass(UAudioComponent().StaticClass()));

	/* Filter */
	bool isStop = false;
	if (!OVRLipSyncPlaybackActor) isStop = IsStop(TEXT("Null_OVRLipSyncPlaybackActor"));
	if (!audio)  isStop = IsStop(TEXT("Null_audio"));
	if (!LipSyncSequence)  isStop = IsStop(TEXT("Null_LipSyncSequence"));
	if (isStop) return;

	/* Apply LipSyncSequence */
	OVRLipSyncPlaybackActor->SetPlaybackSequence(LipSyncSequence);
	OVRLipSyncPlaybackActor->Start(audio, LipSyncSequence);

	UE_LOG(LogTemp, Warning, TEXT("TestTestTest"));
}

bool UAzureTTSComponent::OVRLipSyncProcessSoundWaveAsset(const FAssetData& SoundWaveAsset, bool UseOfflineModel)
{
	auto ObjectPath = SoundWaveAsset.ObjectPath.ToString();
	auto SoundWave = FindObject<USoundWave>(NULL, *ObjectPath);
	bool isFail = false;

	/* isFail */
	bool isStop = false;
	if (!SoundWave) isStop = IsStop((TEXT("Can't find %s"), *ObjectPath));
	if (SoundWave->NumChannels > 2) IsStop((TEXT("Can't process %s: only mono and stereo streams are supported"), *ObjectPath));
	if (isStop) {
		if (DefaultSoundBase) {
			OVRLipSyncProcessSoundBase(DefaultSoundBase);
			return true;
		}
		return false;
	}

	OVRLipSyncProcessSoundWave(SoundWave);
	return true;
}

bool UAzureTTSComponent::OVRLipSyncProcessSoundBase(USoundBase* TargetSoundBase, bool UseOfflineModel)
{
	if (!TargetSoundBase)
	{
		UE_LOG(LogTemp, Error, TEXT("Null property 'USoundBase'."));

		if (DefaultSoundBase) {
			OVRLipSyncProcessSoundBase(DefaultSoundBase);
			return true;
		}
		return false;
	}

	USoundWave* SoundWave = Cast<USoundWave>(TargetSoundBase);
	OVRLipSyncProcessSoundWave(SoundWave);
	return true;
}

bool UAzureTTSComponent::OVRLipSyncProcessSoundWave(USoundWave* TargetSoundWave, bool UseOfflineModel)
{
	if (!TargetSoundWave) {
		UE_LOG(LogTemp, Error, TEXT("Null property 'USoundWave'."));

		if (!DefaultSoundBase) {
			UE_LOG(LogTemp, Error, TEXT("Null property 'DefaultSoundBase'."));
			return false;
		}
	}

	if (LastSoundWave == TargetSoundWave && LipSyncSequence) return true;
	LastSoundWave = TargetSoundWave;

	bool isDecompessed = DecompressSoundWave(TargetSoundWave);
	if(!isDecompessed) return false;

	// Compute LipSync sequence frames at 100 times a second rate
	constexpr auto LipSyncSequenceUpateFrequency = 100;
	constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpateFrequency;

	auto SequenceName = FString::Printf(TEXT("%s_LipSyncSequence"), *TargetSoundWave->GetFName().ToString());
	auto SequencePath = FString::Printf(TEXT("/Game/Audio/TempSound_LipSyncSequence"));
	auto SequencePackage = CreatePackage(*SequencePath);
	auto Sequence = NewObject<UOVRLipSyncFrameSequence>(SequencePackage, *SequenceName, RF_Public | RF_Standalone);
	auto NumChannels = TargetSoundWave->NumChannels;
	auto SampleRate = TargetSoundWave->GetSampleRateForCurrentPlatform();
	auto PCMDataSize = TargetSoundWave->RawPCMDataSize / sizeof(int16_t);
	auto PCMData = reinterpret_cast<int16_t*>(TargetSoundWave->RawPCMData);
	auto ChunkSizeSamples = static_cast<int>(SampleRate * LipSyncSequenceDuration);
	auto ChunkSize = NumChannels * ChunkSizeSamples;

	FString ModelPath = UseOfflineModel ? FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OVRLipSync"),
		TEXT("OfflineModel"), TEXT("ovrlipsync_offline_model.pb"))
		: FString();
	UOVRLipSyncContextWrapper context(ovrLipSyncContextProvider_Enhanced, SampleRate, 4096, ModelPath);

	float LaughterScore = 0.0f;
	int32_t FrameDelayInMs = 0;
	TArray<float> Visemes;

	TArray<int16_t> samples;
	samples.SetNumZeroed(ChunkSize);
	context.ProcessFrame(samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);

	int FrameOffset = (int)(FrameDelayInMs * SampleRate / 1000 * NumChannels);

	FScopedSlowTask SlowTask(PCMDataSize + FrameOffset,
		FText::Format(NSLOCTEXT("NSLT_OVRLipSyncPlugin", "GeneratingLipSyncSequence",
			"Generating LipSync sequence for {0}..."),
			FText::FromName(TargetSoundWave->GetFName())));
	SlowTask.MakeDialog();
	for (int offs = 0; offs < PCMDataSize + FrameOffset; offs += ChunkSize)
	{
		int remainingSamples = PCMDataSize - offs;
		if (remainingSamples >= ChunkSize)
		{
			context.ProcessFrame(PCMData + offs, ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs,
				NumChannels > 1);
		}
		else
		{
			if (remainingSamples > 0)
			{
				memcpy(samples.GetData(), PCMData + offs, sizeof(int16_t) * remainingSamples);
				memset(samples.GetData() + remainingSamples, 0, ChunkSize - remainingSamples);
			}
			else
			{
				memset(samples.GetData(), 0, ChunkSize);
			}
			context.ProcessFrame(samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs,
				NumChannels > 1);
		}

		SlowTask.EnterProgressFrame(ChunkSize);
		if (SlowTask.ShouldCancel())
		{
			return false;
		}
		if (offs >= FrameOffset)
		{
			Sequence->Add(Visemes, LaughterScore);
		}
	}

	LipSyncSequence = Sequence;

	Sequence->MarkPackageDirty();

	return true;
}

bool UAzureTTSComponent::ConvertSoundData(TArray<uint8> Data)
{
	FWaveModInfo WaveInfo;
	if (WaveInfo.ReadWaveInfo(Data.GetData(), Data.Num())) {

		USoundWave* sw = NewObject<USoundWave>();
		int32 DurationDiv = *WaveInfo.pChannels * (*WaveInfo.pBitsPerSample / 8.f) * *WaveInfo.pSamplesPerSec;

		/* Null 필터 */
		{
			if (!sw) {
				UE_LOG(LogTemp, Warning, TEXT("Null Sound Wave"));
				return false;
			}

			if (DurationDiv <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Null Too small than DurationDiv"));
				return false;
			}
		}

		/* rawFile 값을 USoundWave 타입으로 형변환 */
		sw->Duration = WaveInfo.SampleDataSize / DurationDiv;
		sw->SetSampleRate(*WaveInfo.pSamplesPerSec); // 해당 프로퍼티가 클 수록 소리가 빨라짐.
		sw->NumChannels = *WaveInfo.pChannels;
		sw->RawPCMDataSize = WaveInfo.SampleDataSize;
		sw->RawPCMData = (uint8*)FMemory::Malloc(sw->RawPCMDataSize);
		FMemory::Memmove(sw->RawPCMData, Data.GetData(), Data.Num());
		sw->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
		sw->InvalidateCompressedData();

		sw->RawData.Lock(LOCK_READ_WRITE);
		void* LockedData = sw->RawData.Realloc(Data.Num());
		FMemory::Memcpy(LockedData, Data.GetData(), Data.Num());
		sw->RawData.Unlock();

		LastSoundWave = sw;
	}
	return true;
}

bool UAzureTTSComponent::DecompressSoundWave(USoundWave* SoundWave)
{
	if (SoundWave->RawPCMData)
	{
		return true;
	}
	auto AudioDevice = GEngine->GetMainAudioDevice();
	if (!AudioDevice)
	{
		return false;
	}

	AudioDevice->StopAllSounds(true);
	auto OriginalDecompressionType = SoundWave->DecompressionType;
	SoundWave->DecompressionType = DTYPE_Native;
	if (SoundWave->InitAudioResource(AudioDevice->GetRuntimeFormat(SoundWave)))
	{
		USoundWave::FAsyncAudioDecompress Decompress(SoundWave, MONO_PCM_BUFFER_SAMPLES);
		Decompress.StartSynchronousTask();
	}
	SoundWave->DecompressionType = OriginalDecompressionType;

	return true;
}

bool UAzureTTSComponent::IsStop(FString LogStr)
{
	UE_LOG(LogTemp,Warning,TEXT("%s"), *LogStr)
	return true;
}
