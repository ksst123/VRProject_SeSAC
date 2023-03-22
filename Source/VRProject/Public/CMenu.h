// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CMenu.generated.h"

/**
 * 
 */
UCLASS()
class VRPROJECT_API UCMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="MenuEvent")
	void QuitVRGame();
};
