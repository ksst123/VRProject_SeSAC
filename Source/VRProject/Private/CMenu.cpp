// Fill out your copyright notice in the Description page of Project Settings.


#include "CMenu.h"

#include "Kismet/KismetSystemLibrary.h"

void UCMenu::QuitVRGame()
{
	auto PC = GetWorld()->GetFirstPlayerController();
	if(PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, true);
	}
}
