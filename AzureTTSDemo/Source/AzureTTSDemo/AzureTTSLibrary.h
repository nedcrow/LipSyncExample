// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AzureTTSLibrary.generated.h"

/*
* REST API는 Visme 기능을 지원하지 않음 https://techcommunity.microsoft.com/t5/ai-cognitive-services-blog/azure-neural-text-to-speech-extended-to-support-lip-sync-with/ba-p/2356748
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class AZURETTSDEMO_API UAzureTTSLibrary : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAzureTTSLibrary();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
