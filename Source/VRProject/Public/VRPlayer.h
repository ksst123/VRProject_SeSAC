// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "VRPlayer.generated.h"

// 1. 사용자의 입력에 따라 앞뒤좌우로 이동하고 싶다.
// 2. 사용자가 텔레포트 버튼을 눌렀다 떼면 텔레포트 되도록 하고 싶다.
UCLASS()
class VRPROJECT_API AVRPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVRPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "VR Camera")
	class UCameraComponent* VRCamera;

	// motion controller
	UPROPERTY(VisibleAnywhere, Category = "Motion Controller")
	class UMotionControllerComponent* LeftHand;
	UPROPERTY(VisibleAnywhere, Category = "Motion Controller")
	class UMotionControllerComponent* RightHand;

	// 사용할 손 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Controller")
	class USkeletalMeshComponent* LeftHandMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Controller")
	class USkeletalMeshComponent* RightHandMesh;
	
public:
	// 필요속성: 이동속도, 인풋 매핑 컨텍스트, 인풋 액션
	// ============================================================================================ 

	// Move Speed
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float MoveSpeed = 500.f;
	// Input Mapping Context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputMappingContext* IMC_VRInput;
	// Input Action for Move
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* IA_VRMove;
	// Input Action for Look
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* IA_VRLook;
	// 사용할 나이아가라 컴포넌트(LineTrace)
	UPROPERTY(VisibleAnywhere, Category = "Teleport")
	class UNiagaraComponent* TeleportCurveTraceComponent;

	// 이동처리 함수
	void Move(const FInputActionValue& Value);
	// 시선처리 함수
	void Look(const FInputActionValue& Value);
	
	// ============================================================================================

	
	// Teleport Straight
	// ============================================================================================
	
	UPROPERTY(VisibleAnywhere, Category = "Teleport")
	class UNiagaraComponent* TeleportCircle;
	// 텔레포트 입력 액션
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* IA_Teleport;
	// 텔레포트 기능 활성화 여부
	bool bTeleporting = false;
	// 텔레포트 위치
	FVector TeleportPos;

	
	// 텔레포트 버튼을 눌렀을 때 처리할 함수
	void TeleportStart(const FInputActionValue& Value);
	// 텔레포트 버튼을 땠을 때 처리할 함수
	void TeleportEnd(const FInputActionValue& Value);
	// 텔레포트 기능 리셋
	bool TeleportReset();
	// 직선 텔레포트 처리 함수
	void TeleportDrawStraight();
	// 텔레포트 선과 충돌체크 함수
	bool CheckHitTeleport(FVector LastPos, FVector& CurrentPos);
	// 충돌 처리 함수
	bool HitTest(FVector LastPos, FVector CurrentPos, FHitResult& HitInfo);
	
	// ============================================================================================

	
private:
	// 곡선 텔레포트
	// ============================================================================================

	// 곡선 텔레포트 사용 여부
	UPROPERTY(EditAnywhere, Category = "Teleport", meta=(AllowPrivateAccess = true))
	bool bTeleportCurve = true;
	// 던지는 힘
	UPROPERTY(EditAnywhere, Category = "Teleport", meta=(AllowPrivateAccess = true))
	float CurvedPower = 1500.f;
	// 중력
	float Gravity = -5000.f;
	// 시뮬레이션 시간
	UPROPERTY(EditAnywhere, Category = "Teleport", meta=(AllowPrivateAccess = true))
	float SimulatedTime = 0.02f;
	// 곡선을 이루는 점 개수
	UPROPERTY(EditDefaultsOnly, Category = "Teleport", meta=(AllowPrivateAccess = true))
	int32 VertexCount = 40;
	// 점을 기억할 배열
	TArray<FVector> Vertices;

	void TeleportDrawCurve();
	
	
	// ============================================================================================


private:
	// 워프
	// ============================================================================================
	
	// 워프 사용 여부
	UPROPERTY(EditAnywhere, Category = "Teleport", meta=(AllowPrivateAccess = true))
	bool bIsWarp = true;
	// 타이머
	UPROPERTY();
	FTimerHandle WarpHandle;
	// 경과 시간
	UPROPERTY()
	float CurrentTime;
	// 워프할 때 필요한 시간
	UPROPERTY(EditAnywhere, Category = "Teleport", meta=(AllowPrivateAccess = true))
	float WarpTime = 0.2f;

	// 워프를 수행할 함수
	UFUNCTION()
	void DoWarp();
	
	// ============================================================================================


private:
	// 총쏘기
	// ============================================================================================

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta=(AllowPrivateAccess = true))
	class UInputAction* IA_Fire;
	// 집게손가락 표시할 모션 컨트롤러
	UPROPERTY(VisibleAnywhere, Category = "Motion Controller", meta=(AllowPrivateAccess = true))
	class UMotionControllerComponent* RightAim;
	
	// 총쏘기 처리할 함수
	void FireInput(const FInputActionValue& Value);
	
	// ============================================================================================


	// 크로스헤어
	// ============================================================================================

	// 크로스헤어 파일(크로스헤어 공장)
	UPROPERTY(EditAnywhere, Category = "Crosshair", meta=(AllowPrivateAccess = true))
	TSubclassOf<AActor> CrosshairFactory;
	// 크로스헤어 인스턴스
	UPROPERTY()
	AActor* Crosshair;
	// 크로스헤어 그리기
	void DrawCrosshair();

	float NiagaraTime = 0.1f;
	float CurrentNiagaraTime = 0.f;

	// ============================================================================================


private:
	// 물체 잡기/놓기 구현
	// ============================================================================================

	// 잡기 입력 액션
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta=(AllowPrivateAccess=true))
	class UInputAction* IA_Grab;
	// 잡을 범위
	UPROPERTY(EditAnywhere, Category = "Grab")
	float GrabRange = 100.f;
	// 잡은 물체를 기억할 변수
	UPROPERTY()
	class UPrimitiveComponent* GrabbedObject;
	// 잡은 적이 있는 물체가 있었는지 기억할 변수
	bool bIsGrabbed = false;

	// 던지면 원하는 방향으로 날아가도록 하고 싶다.
	// -> 던질 방향
	FVector ThrowDirection;
	// -> 던질 힘
	UPROPERTY(EditAnywhere, Category="Grab")
	float ThrowPower = 1000.f;
	// -> 직전 위치
	FVector PrevPos;
	// 회전 방향
	FQuat DeltaRotation;
	// 이전 회전값
	FQuat PrevRot;
	// 회전 빠르기
	UPROPERTY(EditAnywhere, Category="Grab")
	float ToquePower = 1.f;
	
	// 잡기 시도 기능
	void TryGrab();
	// 물체 놓기 구현
	void TryUnGrab();
	// 잡고 있는 중에 처리할 기능
	void Grabbing();
	
	// ============================================================================================

	
	// 진동
	// ============================================================================================

	UPROPERTY(EditDefaultsOnly, Category="Haptic")
	class UHapticFeedbackEffect_Curve* HF_Fire;
	
	// ============================================================================================

protected:
	// Widget
	// ============================================================================================

	UPROPERTY(VisibleAnywhere, Category="Widget")
	class UWidgetInteractionComponent* WidgetInteractionComponent;
	
	// ============================================================================================

	
public:
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* IMC_Hand;

	
	// 원격 잡기
	// ============================================================================================
	// 원격 잡기 모드가 활성화 되면 원격 잡기 모드를 사용하고 싶다.

	// 원격 잡기 모드 여부
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	bool bIsRemoteGrab = true;
	// 원격 잡기 가능 거리
	UPROPERTY(EditAnywhere, Category="Grab")
	float RemoteGrabDistance = 2000.f;	
	// 잡은 후 끌어당기는 속도
	UPROPERTY(EditAnywhere, Category="Grab")
	float RemotePullSpeed = 10.f;
	// 물체 감지 범위
	UPROPERTY(EditAnywhere, Category="Grab")
	float RemoteRadius = 20.f;
	// 원거리 물체 이동을 위한 타이머
	FTimerHandle RemoteGrabTimer;
	// RemoteGrab 시각화 처리할지 여부
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	bool bDrawDebugGrab = true;
	
	void RemoteGrab();
	// 시각화 처리 함수
	void DrawDebugRemoteGrab();
	
	// ============================================================================================
};
