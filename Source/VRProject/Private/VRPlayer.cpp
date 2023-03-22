// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Components/WidgetInteractionComponent.h"
#include "Haptics/HapticFeedbackEffect_Curve.h"

// Sets default values
AVRPlayer::AVRPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VR Camera"));
	VRCamera->SetupAttachment(RootComponent);
	VRCamera->bUsePawnControlRotation = false;

	LeftHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("Left Hand"));
	LeftHand->SetupAttachment(RootComponent);
	LeftHand->SetTrackingMotionSource(FName("Left"));
	RightHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("Right Hand"));
	RightHand->SetupAttachment(RootComponent);
	RightHand->SetTrackingMotionSource(FName("Right"));

	LeftHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Left Hand Mesh"));
	LeftHandMesh->SetupAttachment(LeftHand);
	RightHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Right Hand Mesh"));
	RightHandMesh->SetupAttachment(RightHand);
	
	ConstructorHelpers::FObjectFinder<USkeletalMesh> TempLeftHandMesh(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/MannequinsXR/Meshes/SKM_MannyXR_left.SKM_MannyXR_left'"));
	if(TempLeftHandMesh.Succeeded())
	{
		LeftHandMesh->SetSkeletalMesh(TempLeftHandMesh.Object);
		LeftHandMesh->SetRelativeLocationAndRotation(FVector(-2.9f,-3.5f,4.5f), FRotator(-25.f,-180.f,90.f));
	}

	ConstructorHelpers::FObjectFinder<USkeletalMesh> TempRightHandMesh(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/MannequinsXR/Meshes/SKM_MannyXR_right.SKM_MannyXR_right'"));
	if(TempRightHandMesh.Succeeded())
	{
		RightHandMesh->SetSkeletalMesh(TempRightHandMesh.Object);
		RightHandMesh->SetRelativeLocationAndRotation(FVector(-2.9f,-3.5f,4.5f), FRotator(25.f,0.f,90.f));
	}

	// Teleport
	TeleportCircle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Teleport Circle"));
	TeleportCircle->SetupAttachment(RootComponent);
	TeleportCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Teleport Trace Niagara
	TeleportCurveTraceComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Teleport Curve Trace"));
	TeleportCurveTraceComponent->SetupAttachment(RootComponent);

	// 집게손가락
	RightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightAim"));
	RightAim->SetupAttachment(RootComponent);
	RightAim->SetTrackingMotionSource(FName("RightAim"));

	// 위젯
	WidgetInteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("Widget Interaction Component"));
	WidgetInteractionComponent->SetupAttachment(RightAim);
}

// Called when the game starts or when spawned
void AVRPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input 사용 처리
	auto PlayerController = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());

	if(PlayerController)
	{
		// LocalPlayer
		auto LocalPlayer = PlayerController->GetLocalPlayer();
		auto Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
		if(Subsystem)
		{
			Subsystem->AddMappingContext(IMC_VRInput, 0);
			Subsystem->AddMappingContext(IMC_Hand, 0);
		}
	}

	TeleportReset();

	// 크로스헤어 객체 만들기
	if(CrosshairFactory)
	{
		Crosshair = GetWorld()->SpawnActor<AActor>(CrosshairFactory);
	}

	// 만약 HMD가 연결되어 있지 않다면
	if(UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() == false)
	{
		// Hand를 테스트 할 수 있는 위치로 이동시킨다.
		RightHand->SetRelativeLocation(FVector(20.f, 20.f, 0.f));
		RightAim->SetRelativeLocation(FVector(20.f, 20.f, 0.f));
		// 카메라의 Use Pawn Control Rotation을 활성화 시킨다.
		VRCamera->bUsePawnControlRotation = true;
	}
	// 만약 HMD가 연결되어 있다면
	else
	{
		// 기본 트래킹 오프셋 설정
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Eye);
	}
}

// Called every frame
void AVRPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// HMD가 연결되어 있지 않으면
	if(UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() == false)
	{
		// 손이 카메라 방향과 일치하도록 한다.
		RightHand->SetRelativeRotation(VRCamera->GetRelativeRotation());
		RightAim->SetRelativeRotation(VRCamera->GetRelativeRotation());
	}
	
	
	// 텔레포트 확인 처리
	if(bTeleporting)
	{
		if(bTeleportCurve)
		{
			// 곡선 텔레포트
			TeleportDrawCurve();
		}
		else
		{
			// 직선 텔레포트
			TeleportDrawStraight();
		}
	}

	CurrentNiagaraTime += DeltaTime;
	if(CurrentNiagaraTime > NiagaraTime)
	{
		// 나이아가라를 이용해 선 그리기
		if(TeleportCurveTraceComponent)
		{
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(TeleportCurveTraceComponent, FName("User.PointArray"), Vertices);
		}
		CurrentNiagaraTime = 0.f;
	}

	// Crosshair
	DrawCrosshair();

	Grabbing();

	DrawDebugRemoteGrab();
}

// Called to bind functionality to input
void AVRPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto InputSystem = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	if(InputSystem)
	{
		// Binding
		InputSystem->BindAction(IA_VRMove, ETriggerEvent::Triggered, this, &AVRPlayer::Move);
		InputSystem->BindAction(IA_VRLook, ETriggerEvent::Triggered, this, &AVRPlayer::Look);

		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Started, this, &AVRPlayer::TeleportStart);
		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Completed, this, &AVRPlayer::TeleportEnd);

		InputSystem->BindAction(IA_Fire, ETriggerEvent::Started, this, &AVRPlayer::FireInput);

		InputSystem->BindAction(IA_Grab, ETriggerEvent::Started, this, &AVRPlayer::TryGrab);
		InputSystem->BindAction(IA_Grab, ETriggerEvent::Completed, this, &AVRPlayer::TryUnGrab);
	}
}

void AVRPlayer::Move(const FInputActionValue& Value)
{
	// 1. 사용자의 입력에 따라 이동한다.
	FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.X);
	AddMovementInput(GetActorRightVector(), Axis.Y);

	// // 1. 사용자의 입력에 따라
	// FVector2D Axis = Value.Get<FVector2D>();
	// // 2. 앞뒤좌우라는 방향으로
	// FVector dir(Axis.X, Axis.Y, 0);
	// // 3. 이동한다. ( P = P0 + vt)
	// FVector P0 = GetActorLocation();
	// FVector vt = dir * MoveSpeed * GetWorld()->DeltaTimeSeconds;
	// FVector P = P0 + vt;
	// SetActorLocation(P);
}

void AVRPlayer::Look(const FInputActionValue& Value)
{
	FVector2D Axis = Value.Get<FVector2D>();
	AddControllerPitchInput(-1 * Axis.Y);
	AddControllerYawInput(Axis.X);
}

// 텔레포트 기능 활성화 처리
void AVRPlayer::TeleportStart(const FInputActionValue& Value)
{
	// 1. 텔레포트 이동
	// 2. 텔레포트 목적지
	// 3. 사용자가 그 지점을 가리킨다
	// 4. 텔레포트 버튼을 눌렀다 뗐기 때문에

	// 텔레포트 버튼을 눌렀을 때는 사용자가 어디를 가리키는 지 주시하고 싶다.
	bTeleporting = true;
	// 라인이 보이도록 활성화
	TeleportCurveTraceComponent->SetVisibility(true);
}


void AVRPlayer::TeleportEnd(const FInputActionValue& Value)
{
	// 텔레포트 기능 리셋
	bTeleporting = false;
	// 만약 텔레포트가 불가능하다면 
	if(!TeleportReset())
	{
		// 다음 처리를 하지 않는다.
		return;
	}
	
	// 워프 사용 시 워프 처리
	if(bIsWarp)
	{
		DoWarp();
		return;
	}
	// 그렇지 않을 경우 텔레포트 처리
	else
	{
		// 텔레포트 위치로 이동하고 싶다.
		SetActorLocation(TeleportPos + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
}

bool AVRPlayer::TeleportReset()
{
	// TeleportCircle이 활성화 되어 있을 때만 텔레포트가 가능하다.
	bool bCanTeleport = TeleportCircle->GetVisibleFlag();
	// TeleportCircle을 안보이게 처리
	TeleportCircle->SetVisibility(false);
	TeleportCurveTraceComponent->SetVisibility(false);
	bTeleporting = false;
	
	return bCanTeleport;
}

void AVRPlayer::TeleportDrawStraight()
{
	Vertices.RemoveAt(0, Vertices.Num());
	
	// 직선을 그리고 싶다.
	// 필요정보: 시작점, 종료점
	FVector StartPos = RightAim->GetComponentLocation();
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 1000.f;

	// 두 점 사이에 충돌체가 있는지 확인한다.
	CheckHitTeleport(StartPos, EndPos);
	Vertices.Add(StartPos);
	Vertices.Add(EndPos);

	// 직선을 그린다.
	// DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Red, false, -1.f, 0.f, 1.f);
}

bool AVRPlayer::CheckHitTeleport(FVector LastPos, FVector& CurrentPos)
{
	FHitResult HitInfo;
	bool bHit = HitTest(LastPos, CurrentPos, HitInfo);
	// 만약 충돌한 대상이 바닥이라면
	if(bHit && HitInfo.GetActor()->GetName().Contains(TEXT("Floor")))
	{
		// 마지막 점(EndPos)을 최종점으로 수정하고 싶다.
		CurrentPos = HitInfo.Location;
		// TeleportCircle 활성화
		TeleportCircle->SetVisibility(true);
		// TeleportCircle을 위치시킨다.
		TeleportCircle->SetWorldLocation(CurrentPos);
		TeleportPos = CurrentPos;
	}
	else
	{
		TeleportCircle->SetVisibility(false);
	}

	return bHit;
}

bool AVRPlayer::HitTest(FVector LastPos, FVector CurrentPos, FHitResult& HitInfo)
{
	// 충돌에서 자기 자신은 무시한다.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	
	// 충돌 여부 확인
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, LastPos, CurrentPos, ECollisionChannel::ECC_Visibility, Params);

	return bHit;
}

// 주어진 속도로 투사체를 날려보내고, 투사체의 지나간 점을 기록한다.
void AVRPlayer::TeleportDrawCurve()
{
	// Vertices 초기화
	Vertices.RemoveAt(0, Vertices.Num());
	
	// 1. 시작점, 방향, 세기를 가지고 
	FVector CurvePos = RightAim->GetComponentLocation();
	FVector Dir = RightAim->GetForwardVector() * CurvedPower;

	Vertices.Add(CurvePos);
	for(int i = 0; i < VertexCount-1; i++)
	{
		// 이전 점 기억
		FVector LastCurvePos = CurvePos;
		// 1. 투사체가 이동했으니까 반복적으로
		// v = v0 + at
		Dir += FVector::UpVector * Gravity * SimulatedTime;
		// P = P0 + vt
		CurvePos += Dir * SimulatedTime;
		// 2. 투사체의 위치에서
		// 만약 점과 점 사이에 물체가 가로막고 있다면
		if(CheckHitTeleport(LastCurvePos, CurvePos))
		{
			// 그 점을 마지막 점으로 한다.
			Vertices.Add(CurvePos);
			break;
		}
		
		
		// 3. 점을 기록한다.
		Vertices.Add(CurvePos);
	}

	// for(int i = 0; i < Vertices.Num()-1; i++)
	// {
	// 	DrawDebugLine(GetWorld(), Vertices[i], Vertices[i+1], FColor::Yellow, false, -1.f, 0.f, 1.f);
	// }
	
}

void AVRPlayer::DoWarp()
{
	// 만약 워프 기능이 활성화 되어 있지 않다면
	if(bIsWarp == false)
	{
		// 함수 종료
		return;
	}
	// 워프 기능이 활성화 되어 있다면
	// 워프를 수행하고 싶다.
	// -> 일정 시간 동안 빠르게 이동하는 것

	// 경과 시간 초기화
	CurrentTime = 0.f;
	// 충돌체 비활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 1. 시간이 흘러야 한다.
	
	// 2. 일정 시간 동안
	// Lambda: [캡처](parameter)->returnType{body}, 캡처 안에 변수/함수를 모두 사용 가능
	GetWorld()->GetTimerManager().SetTimer(WarpHandle, FTimerDelegate::CreateLambda([this]()->void
	{
		// function body
		//일정 시간 안에 목적지에 도착하고 싶다.
		// 1. 시간이 흘러야 한다.
		CurrentTime += GetWorld()->DeltaTimeSeconds;
		// 현재 위치
		FVector CurrentPos = GetActorLocation();
		// 도착 위치
		FVector EndPos = TeleportPos + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		// 2. 이동해야 한다.
		CurrentPos = FMath::Lerp<FVector>(CurrentPos, EndPos, CurrentTime / WarpTime);
		
		// // 3. 목적지에 도착
		// // 거리가 거의 가까워졌다면 그 위치로 할당한다.
		// float Distance = FVector::Dist(CurrentPos, EndPos);
		// if(Distance < 0.1f)
		// {
		// 	CurrentPos = EndPos;
		// }
		// SetActorLocation(CurrentPos);
		// // 충돌체 활성화
		// GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 3. 목적지에 도착
		SetActorLocation(CurrentPos);
		// 시간이 다 흘렀다면
		if(CurrentTime >= WarpTime)
		{
			// -> 그 위치로 할당하고
			SetActorLocation(EndPos);
			// -> 타이머 종료해주기
			GetWorld()->GetTimerManager().ClearTimer(WarpHandle);
		}
	}), 0.01f, true);
	// 충돌체 활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AVRPlayer::FireInput(const FInputActionValue& Value)
{
	// UI에 이벤트를 전달하고 싶다.
	if(WidgetInteractionComponent)
	{
		WidgetInteractionComponent->PressPointerKey(FKey(FName("LeftMouseButton")));
	}
	
	// 진동처리 하고 싶다.
	auto PC = Cast<APlayerController>(GetController());
	if(PC)
	{
		PC->PlayHapticEffect(HF_Fire, EControllerHand::Right);
	}
	
	// LineTrace를 이용해서 총을 쏘고 싶다.
	// 시작점
	FVector StartPos = RightAim->GetComponentLocation();
	// 종료점
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 100000.f;
	// 총쏘기(LineTrace 동작)
	FHitResult HitInfo;
	bool bHit = HitTest(StartPos, EndPos, HitInfo);
	// 만약 부딪힌 대상이 있으면 
	if(bHit)
	{
		auto HitComp = HitInfo.GetComponent();
		if(HitComp && HitComp->IsSimulatingPhysics())
		{
			// 대상을 날려보낸다.
			HitComp->AddForceAtLocation((EndPos - StartPos).GetSafeNormal() * HitComp->GetMass() * 100000.f, HitInfo.Location);
		}
	}
}

// 거리에 따라서 크로스헤어 크기가 같게 보이도록 한다.
void AVRPlayer::DrawCrosshair()
{
	// 시작점
	FVector StartPos = RightAim->GetComponentLocation();
	// 끝점
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000.f;
	// 거리 저장
	float Distance = 0.f;
	// 충돌 정보를 저장
	FHitResult HitInfo;
	// 충돌 체크
	bool bHit = HitTest(StartPos, EndPos, HitInfo);
	// 충돌이 발생하면
	if(bHit)
	{
		// -> 충돌한 지점에 크로헤어 표시
		Crosshair->SetActorLocation(HitInfo.Location);
		Distance = HitInfo.Distance;
	}
	// 그렇지 않으면
	else
	{
		// -> 그냥 끝점에 크로스헤어 표시;
		Crosshair->SetActorLocation(EndPos);
		Distance = (EndPos - StartPos).Size();
	}

	Crosshair->SetActorScale3D(FVector(FMath::Max<float>(1, Distance)));

	// 빌보딩
	// -> 크로스헤어가 카메라를 바라보도록 처리
	FVector Direction = Crosshair->GetActorLocation() - VRCamera->GetComponentLocation();
	Crosshair->SetActorRotation(Direction.Rotation());
}

void AVRPlayer::TryGrab()
{
	// 원거리 잡기가 활성화 되어 있으면
	if(bIsRemoteGrab)
	{
		// 원거리 잡기를 호출하고
		RemoteGrab();
		// 아래 잡기는 처리하지 않는다.
		return;
	}

	
	// 중심점
	FVector CenterPoint = RightHand->GetComponentLocation();
	// 충돌한 물체들을 기록할 배열
	TArray<FOverlapResult> HitObjs;
	// 충돌 질의 작성
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightHand);
	// 충돌 체크(구 충돌)
	bool bHit = GetWorld()->OverlapMultiByChannel(HitObjs, CenterPoint, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(GrabRange), Params);

	// 만약 충돌하지 않았다면
	if(bHit == false)
	{
		// 아무 처리하지 않는다.
		return;
	}
	
	// 가장 가까운 물체의 배열 인덱스
	int32 Closest = 0;
	for(int32 i = 0; i < HitObjs.Num(); i++)
	{
		// 1. 물리 기능이 활성화 되어 있는지 물체들 중에서
		// -> 만약 부딪힌 컴포넌트가 물리기능이 비활성화 되어 있다면
		// -> 검출하고 싶지 않다.
		if(HitObjs[i].GetComponent()->IsSimulatingPhysics() == false)
		{
			continue;
		}
		// 잡았다
		bIsGrabbed = true;
		// 현재 가장 가까운 물체와 RightHand 와의 거리
		float ClosestDist = FVector::Dist(HitObjs[Closest].GetActor()->GetActorLocation(), CenterPoint);
		// 다음에 검출할 물체와 RightHand 와의 거리
		float NestDist = FVector::Dist(HitObjs[i].GetActor()->GetActorLocation(), CenterPoint);

		// 2. 현재 RightHand로부터 현재 가장 가까운 물체와 이번에 검출할 물체 중 더 가까운 물체가 있다면
		if(ClosestDist > NestDist)
		{
			// 3. 둘 중 더 가까운 것을 가장 가까운 물체로 변경
			Closest = i;
		}
	}

	// 만약 물체를 잡았다면
	if(bIsGrabbed)
	{
		GrabbedObject = HitObjs[Closest].GetComponent();
		// 물체 물리 기능 비활성화
		GrabbedObject->SetSimulatePhysics(false);
		GrabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// RightHand에 붙인다
		GrabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);

		PrevPos = RightHand->GetComponentLocation();
		PrevRot = RightHand->GetComponentQuat();
	}
}

void AVRPlayer::TryUnGrab()
{
	// 만약 잡고있는 물체가 없다면
	if(bIsGrabbed == false)
	{
		// 기능 실행 X
		return;
	}

	// 놓고 싶다.
	// 1. 잡지 않은 상태로 전환한다.
	bIsGrabbed = false;
	// 2. 손에서 물체를 떼어낸다.
	GrabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// 3. 물체의 물리 기능 다시 활성화
	GrabbedObject->SetSimulatePhysics(true);
	// 4. 물체의 충돌 기능 다시 활성화
	GrabbedObject->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 5. 던진다.
	GrabbedObject->AddForce(ThrowDirection * ThrowPower * GrabbedObject->GetMass());

	// 회전 시키기
	// 각속도 = (1 / dt) * dTheta(특정 축 기준 변위 각도 Axis, Angle)
	float Angle;
	FVector Axis;
	float dt = GetWorld()->DeltaTimeSeconds;
	DeltaRotation.ToAxisAndAngle(Axis, Angle);
	FVector AngularVelocity = (1 / dt) * Angle * Axis;
	GrabbedObject->SetPhysicsAngularVelocityInRadians(AngularVelocity * ToquePower, true);

	GrabbedObject = nullptr;
}

// 던질 때 필요한 정보를 업데이트 하기 위한 기능
void AVRPlayer::Grabbing()
{
	if(bIsGrabbed == false)
	{
		return;
	}

	// 던질 방향을 계속 업데이트
	ThrowDirection = RightHand->GetComponentLocation() - PrevPos;
	// 이전 위치 업데이트
	PrevPos = RightHand->GetComponentLocation();

	// 회전 방향 업데이트
	
	// 쿼터니언 공식
	// Angle = Q1, Angle2 = Q2
	// Angle1 + Angle2 = Q1 * Q2;
	// -Angle1 = Q1.Inverse()
	// Angle2 <- Angle1 == Angle2 - Angle1 = Q2 * Q1.Inverse()
	DeltaRotation = RightHand->GetComponentQuat() * PrevRot.Inverse();
	//이전 회전값 업데이트
	PrevRot = RightHand->GetComponentQuat();

	
}

void AVRPlayer::RemoteGrab()
{
	// 중심점
	FVector CenterPoint = RightAim->GetComponentLocation();
	// 트레이스 최대 거리
	FVector EndPos = CenterPoint + RightAim->GetForwardVector() * RemoteGrabDistance;
	// 충돌한 물체들을 기록할 배열
	TArray<FOverlapResult> HitObjs;
	// 충돌 질의 작성
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightAim);

	FHitResult HitInfo;
	bool bHit = GetWorld()->SweepSingleByChannel(HitInfo, CenterPoint, EndPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(RemoteRadius), Params);

	// 충돌이 됐으면 잡아당기기 애니메이션 실행
	if(bHit && HitInfo.GetComponent()->IsSimulatingPhysics())
	{
		// 잡았다
		bIsGrabbed = true;
		// 잡은 물체 할당
		GrabbedObject = HitInfo.GetComponent();
		// 물체 물리기능 비활성화
		GrabbedObject->SetSimulatePhysics(false);
		GrabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// RightHand에 붙인다
		GrabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);



		// 잡은 원거리 물체가 손으로 끌려오도록 처리
		GetWorldTimerManager().SetTimer(RemoteGrabTimer, FTimerDelegate::CreateLambda([this]()->void
		{
			// 이동 중간에 사용자가 놔버리면
			if(GrabbedObject == nullptr)
			{
				GetWorldTimerManager().ClearTimer(RemoteGrabTimer);
				return;
			}
			
			// 물체가 손 위치로 점차 다가오며 도착
			FVector Pos = GrabbedObject->GetComponentLocation();
			FVector TargetPos = RightHand->GetComponentLocation() + RightHand->GetForwardVector() * 100.f;
			Pos = FMath::Lerp<FVector>(Pos, TargetPos, RemotePullSpeed * GetWorld()->DeltaTimeSeconds);
			GrabbedObject->SetWorldLocation(Pos);

			// 목표에 거의 가까워졌다면
			float Distance = FVector::Dist(Pos, TargetPos);
			if(Distance < 10.f)
			{
				// 이동 중단하기
				GrabbedObject->SetWorldLocation(TargetPos);

				PrevPos = RightHand->GetComponentLocation();
				PrevRot = RightHand->GetComponentQuat();
				
				GetWorldTimerManager().ClearTimer(RemoteGrabTimer);
			}
		}
		), 0.02f, true);
		
	}
}

void AVRPlayer::DrawDebugRemoteGrab()
{
	// 시각화 할 지 여부 확인 및 원거리 잡기 활성화 여부 확인
	if(bDrawDebugGrab == false && bIsRemoteGrab == false)
	{
		return;
	}

	// 중심점
	FVector StartPos = RightAim->GetComponentLocation();
	// 트레이스 최대 거리
	FVector EndPos = StartPos + RightAim->GetForwardVector() * RemoteGrabDistance;
	// 충돌한 물체들을 기록할 배열
	TArray<FOverlapResult> HitObjs;
	// 충돌 질의 작성
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightAim);

	FHitResult HitInfo;
	bool bHit = GetWorld()->SweepSingleByChannel(HitInfo, StartPos, EndPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(RemoteRadius), Params);

	DrawDebugSphere(GetWorld(), StartPos, RemoteRadius, 10.f, FColor::Yellow);
	if(bHit)
	{
		// 그리기
		DrawDebugSphere(GetWorld(), HitInfo.Location, RemoteRadius, 10.f, FColor::Yellow);
	}

}
