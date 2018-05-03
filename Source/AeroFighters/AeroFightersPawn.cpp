// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AeroFightersPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "AirProjectile.h"

AAeroFightersPawn::AAeroFightersPawn()
{
	// Structure to hold one-time initialization
	struct FConstructorStatics {
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> PlaneMesh;
		FConstructorStatics() : PlaneMesh(TEXT("/Game/Fighter/DarkFighter.DarkFighter")) { }
	};
	static FConstructorStatics ConstructorStatics;

	// Create static mesh component
	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh0"));
	PlaneMesh->SetStaticMesh(ConstructorStatics.PlaneMesh.Get());	// Set static mesh
	RootComponent = PlaneMesh;

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->SetupAttachment(RootComponent);	// Attach SpringArm to RootComponent
	SpringArm->TargetArmLength = 350.0f; // The camera follows at this distance behind the character	
	SpringArm->SocketOffset = FVector(0.f,0.f,10.f);
	SpringArm->bEnableCameraLag = false;	// Do not allow camera to lag
	SpringArm->CameraLagSpeed = 15.f;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);	// Attach the camera
	Camera->bUsePawnControlRotation = false; // Don't rotate camera with controller
	
	LeftMuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("LeftMuzzleLocation"));
	LeftMuzzleLocation->SetupAttachment(RootComponent);
	LeftMuzzleLocation->SetRelativeLocation(FVector(550.0f, -650.0f, -50.0f));
	LeftMuzzleLocation->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	
	RightMuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("RightMuzzleLocation"));
	RightMuzzleLocation->SetupAttachment(RootComponent);
	RightMuzzleLocation->SetRelativeLocation(FVector(550.0f, 650.0f, -50.0f));
	RightMuzzleLocation->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	
	// Set handling parameters
	Acceleration = 500.f;
	TurnSpeed = 50.f;
	MaxSpeed = 4000.f;
	MinSpeed = 500.f;
	CurrentForwardSpeed = 500.f;
	IsTurning = false;
	GunOffset = FVector(200.f, 0.f, 0.f);
	FireRate = 0.1f;
	CanFire = true;
}

void AAeroFightersPawn::Tick(float DeltaSeconds)
{
	const FVector LocalMove = FVector(CurrentForwardSpeed * DeltaSeconds, 0.f, 0.f);

	// Move plan forwards (with sweep so we stop when we collide with things)
	AddActorLocalOffset(LocalMove, true);

	// Calculate change in rotation this frame
	FRotator DeltaRotation(0,0,0);
	DeltaRotation.Pitch = CurrentPitchSpeed * DeltaSeconds;
	DeltaRotation.Yaw = CurrentYawSpeed * DeltaSeconds;
	if (IsTurning) {
		DeltaRotation.Roll = CurrentRollSpeed * DeltaSeconds;
		IsTurning = false;
	}
	
	// Rotate plane
	AddActorLocalRotation(DeltaRotation);

	// Call any parent class Tick implementation
	Super::Tick(DeltaSeconds);
}

void AAeroFightersPawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// Deflect along the surface when we collide.
	FRotator CurrentRotation = GetActorRotation();
	SetActorRotation(FQuat::Slerp(CurrentRotation.Quaternion(), HitNormal.ToOrientationQuat(), 0.025f));
}


void AAeroFightersPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
    // Check if PlayerInputComponent is valid (not NULL)
	check(PlayerInputComponent);

	// Bind our control axis' to callback functions
	PlayerInputComponent->BindAxis("Thrust", this, &AAeroFightersPawn::ThrustInput);
	PlayerInputComponent->BindAxis("MoveUp", this, &AAeroFightersPawn::MoveUpInput);
	PlayerInputComponent->BindAxis("LoopRight", this, &AAeroFightersPawn::LoopRightInput);
	PlayerInputComponent->BindAxis("TurnRight", this, &AAeroFightersPawn::TurnRightInput);
	PlayerInputComponent->BindAxis("FireWeapon", this, &AAeroFightersPawn::OnFire);
}

void AAeroFightersPawn::ThrustInput(float Val)
{
	// Is there any input?
	bool bHasInput = !FMath::IsNearlyEqual(Val, 0.f);
	// If input is not held down, reduce speed
	float CurrentAcc = bHasInput ? (Val * Acceleration) : (-0.5f * Acceleration);
	// Calculate new speed
	float NewForwardSpeed = CurrentForwardSpeed + (GetWorld()->GetDeltaSeconds() * CurrentAcc);
	// Clamp between MinSpeed and MaxSpeed
	CurrentForwardSpeed = FMath::Clamp(NewForwardSpeed, MinSpeed, MaxSpeed);
}

void AAeroFightersPawn::MoveUpInput(float Val)
{
	// Target pitch speed is based in input
	float TargetPitchSpeed = (Val * TurnSpeed * -1.f);

	// When steering, we decrease pitch slightly
	TargetPitchSpeed += (FMath::Abs(CurrentYawSpeed) * -0.2f);

	// Smoothly interpolate to target pitch speed
	CurrentPitchSpeed = FMath::FInterpTo(CurrentPitchSpeed, TargetPitchSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void AAeroFightersPawn::LoopRightInput(float Val)
{
	const bool bIsRolling = FMath::Abs(Val) > 0.2f;

	// If turning, yaw value is used to influence roll
	// If not turning, roll to reverse current roll value.
	if (bIsRolling && !IsTurning) {
		FRotator rotator = GetActorRotation();
		float componentForX = rotator.GetComponentForAxis(EAxis::X);
		rotator.SetComponentForAxis(EAxis::X, componentForX + 1.0f * Val);
		SetActorRotation(rotator);
	}
}

void AAeroFightersPawn::TurnRightInput(float Val) {
	// Target yaw speed is based on input
	float TargetYawSpeed = (Val * TurnSpeed);

	// Smoothly interpolate to target yaw speed
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, GetWorld()->GetDeltaSeconds(), 2.f);

	IsTurning = FMath::Abs(Val) > 0.2f;
	// If turning, yaw value is used to influence roll
	// If not turning, roll to reverse current roll value.
	float TargetRollSpeed = IsTurning ? (CurrentYawSpeed * 0.5f) : (GetActorRotation().Roll * -2.f);

	// Smoothly interpolate roll speed
	CurrentRollSpeed = FMath::FInterpTo(CurrentRollSpeed, TargetRollSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void AAeroFightersPawn::OnFire(float Val) {
	// try and fire a projectile
	const bool IsFiring = FMath::Abs(Val);
	if (ProjectileClass != NULL && IsFiring && CanFire)
	{
		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			const FRotator LeftSpawnRotation = LeftMuzzleLocation->GetComponentRotation();
			const FVector LeftSpawnLocation = LeftMuzzleLocation->GetComponentLocation() + GunOffset;
		
			const FRotator RightSpawnRotation = RightMuzzleLocation->GetComponentRotation();
			const FVector RightSpawnLocation = RightMuzzleLocation->GetComponentLocation() + GunOffset;

			World->SpawnActor<AAirProjectile>(ProjectileClass, LeftSpawnLocation, LeftSpawnRotation);
			World->SpawnActor<AAirProjectile>(ProjectileClass, RightSpawnLocation, RightSpawnRotation);

			CanFire = false;
			World->GetTimerManager().SetTimer(TimerHandle_ShotTimerExpired, this, &AAeroFightersPawn::ShotTimerExpired, FireRate);
		}
	}
}

void AAeroFightersPawn::ShotTimerExpired()
{
	CanFire = true;
}
