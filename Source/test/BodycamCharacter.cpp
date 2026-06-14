// First-person bodycam character implementation.

#include "BodycamCharacter.h"
#include "BodycamCameraSystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputActionValue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	// AnimBP variables can compile as float or double depending on how they were
	// authored, so resolve the property generically.
	void SetAnimFloat(UAnimInstance* AnimInst, const FName Name, const float Value)
	{
		if (!AnimInst)
		{
			return;
		}
		if (const FFloatProperty* FP = FindFProperty<FFloatProperty>(AnimInst->GetClass(), Name))
		{
			FP->SetPropertyValue_InContainer(AnimInst, Value);
		}
		else if (const FDoubleProperty* DP = FindFProperty<FDoubleProperty>(AnimInst->GetClass(), Name))
		{
			DP->SetPropertyValue_InContainer(AnimInst, static_cast<double>(Value));
		}
	}
}

ABodycamCharacter::ABodycamCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false; // per-view-mode policy applied in ApplyViewMode()
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		Move->MaxWalkSpeed = WalkSpeed;
		Move->MaxWalkSpeedCrouched = WalkSpeed * 0.5f;
		Move->JumpZVelocity = 430.f;
		Move->AirControl = 0.35f;
		// Soft accel/brake so starting and stopping feel organic, matching the
		// anim crossfades instead of snapping to a halt.
		Move->MaxAcceleration = 1200.f;
		Move->BrakingDecelerationWalking = 2048.f;
		Move->BrakingFrictionFactor = 0.9f;
		Move->GroundFriction = 6.f;
		Move->NavAgentProps.bCanCrouch = true;
	}

	// ---------- Camera rig ----------
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, CameraHeight);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 22.f;
	// The body mesh gets rescaled at runtime; never let that scale leak into the camera.
	CameraBoom->SetAbsolute(false, false, /*scale*/ true);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(BaseFOV);

	// Clean image — no colour grading. All bodycam feel comes from motion,
	// produced by the dedicated camera system (SCP:CB / VotV style).
	CameraSystem = CreateDefaultSubobject<UBodycamCameraSystem>(TEXT("CameraSystem"));
	CameraSystem->BaseFOV = BaseFOV;
	CameraSystem->SprintFOV = SprintFOV;
	CameraSystem->ReferenceWalkSpeed = WalkSpeed;

	// ---------- Body mesh ----------
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		MeshComp->SetOwnerNoSee(false); // bodycam: the player must see their own body
		// The head is never hidden now (the lens stays in front of it), so the body
		// mesh casts its own full shadow and no shadow twin is needed.
		MeshComp->SetCastShadow(true);

		// Runtime-assigned meshes can get frustum-culled while animating; keep the
		// pose (and bounds) refreshing even off-screen.
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		MeshComp->bEnableUpdateRateOptimizations = false;
		MeshComp->SetBoundsScale(2.f);

		static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshObj(
			TEXT("/Game/Character/SK_Hunyuan.SK_Hunyuan"));
		if (MeshObj.Succeeded())
		{
			MeshComp->SetSkeletalMeshAsset(MeshObj.Object);
		}
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPClass(
		TEXT("/Game/Character/ABP_Bodycam"));
	if (AnimBPClass.Succeeded())
	{
		BodyAnimClass = AnimBPClass.Class;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> AnimObj(
		TEXT("/Game/Character/A_Walk.A_Walk"));
	if (AnimObj.Succeeded())
	{
		WalkAnim = AnimObj.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleObj(
		TEXT("/Game/Character/A_Idle.A_Idle"));
	if (IdleObj.Succeeded())
	{
		IdleAnim = IdleObj.Object;
	}

	// ---------- Enhanced Input assets, created in code ----------
	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;
	SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;
	CrouchAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Crouch"));
	CrouchAction->ValueType = EInputActionValueType::Boolean;
	ToggleViewAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_ToggleView"));
	ToggleViewAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Default"));

	// WASD -> 2D vector. Default key value lands on X; swizzle YXZ moves it to Y for fwd/back.
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("Mod_W_Swz")));
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("Mod_S_Swz")));
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("Mod_S_Neg")));
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("Mod_A_Neg")));
	}
	DefaultMappingContext->MapKey(MoveAction, EKeys::D);

	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	DefaultMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	DefaultMappingContext->MapKey(ToggleViewAction, EKeys::V);
}

void ABodycamCharacter::ApplyViewMode()
{
	if (!CameraBoom)
	{
		return;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* MeshComp = GetMesh();

	if (bThirdPerson)
	{
		// Orbit camera, body faces movement direction.
		bUseControllerRotationYaw = false;
		if (Move)
		{
			Move->bOrientRotationToMovement = true;
		}

		CameraBoom->AttachToComponent(GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		CameraBoom->SetRelativeLocation(FVector::ZeroVector);
		CameraBoom->TargetArmLength = CameraDistance;
		CameraBoom->SocketOffset = FVector(0.f, 0.f, CameraHeight);
		CameraBoom->bDoCollisionTest = true;
		CameraBoom->bEnableCameraLag = true;
		CameraBoom->bEnableCameraRotationLag = true;
		CameraBoom->CameraLagSpeed = 12.f;
		CameraBoom->CameraRotationLagSpeed = 14.f;

		if (FollowCamera)
		{
			FollowCamera->SetRelativeLocation(FVector::ZeroVector);
			FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	else
	{
		// Bodycam: body yaw follows the camera.
		bUseControllerRotationYaw = true;
		if (Move)
		{
			Move->bOrientRotationToMovement = false;
		}

		// The boom keeps the player's AIM rotation, but its world POSITION is driven
		// every tick by UpdateFirstPersonCamera so the lens rides ~20cm IN FRONT of
		// the head bone and follows its forward lunge. The head therefore always sits
		// behind the lens (out of frame, behind the near plane) without hiding it —
		// so it still casts a full shadow — and the body can never overtake the lens.
		// SocketOffset/arm length stay 0.
		CameraBoom->AttachToComponent(GetCapsuleComponent(),
			FAttachmentTransformRules::KeepWorldTransform);
		CameraBoom->SocketOffset = FVector::ZeroVector;
		CameraBoom->TargetArmLength = 0.f;
		CameraBoom->bDoCollisionTest = false;
		bFPCamSeeded = false; // re-seed the follow on entering first person
		// NO positional or rotational lag in first person: the lens stays glued
		// to the head so walking never leaves the camera behind. All handheld
		// weight comes from the camera system's springs instead.
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
	}
}

void ABodycamCharacter::ToggleView()
{
	bThirdPerson = !bThirdPerson;
	ApplyViewMode();
}

void ABodycamCharacter::UpdateFirstPersonCamera(float Dt)
{
	if (bThirdPerson || !CameraBoom || !GetCapsuleComponent())
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Fwd = YawOnly.RotateVector(FVector::ForwardVector);

	const FVector Head = MeshComp->GetBoneLocation(HeadBoneName, EBoneSpaces::WorldSpace);
	if (Head.IsNearlyZero())
	{
		// Fallback to a capsule eye point if the head bone can't be read this frame.
		FPCamPos = GetCapsuleComponent()->GetComponentLocation()
			+ FVector(0.f, 0.f, FirstPersonEyeHeight) + Fwd * FirstPersonEyeForward;
		CameraBoom->SetWorldLocation(FPCamPos);
		return;
	}

	// HARD bind: the lens is locked exactly in front of (and a touch above) the head
	// bone every frame, no smoothing. Wherever the head moves — bob, lean, run lunge —
	// the lens moves identically, so the head (and body) can never reach the lens.
	// Aim still comes from the mouse (boom uses pawn control rotation); only the
	// position rides the head, which is the handheld bodycam feel.
	FPCamPos = Head + Fwd * FirstPersonEyeForward + FVector(0.f, 0.f, FirstPersonEyeUpFromHead);
	CameraBoom->SetWorldLocation(FPCamPos);
}

void ABodycamCharacter::SetupBodyAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;
	}

	if (BodyAnimClass)
	{
		MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		MeshComp->SetAnimInstanceClass(BodyAnimClass);
		BodyAnim = MeshComp->GetAnimInstance();
		bUsingAnimBP = (BodyAnim != nullptr);
	}

	if (!bUsingAnimBP && WalkAnim)
	{
		// Fallback: raw clip swapping (no blending) if the AnimBP is missing.
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComp->InitAnim(true);
		MeshComp->PlayAnimation(WalkAnim, /*bLooping*/ true);
		MeshComp->SetPlayRate(1.f);
		bAnimReady = true;
	}
}

void ABodycamCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CameraSystem)
	{
		CameraSystem->Init(FollowCamera);
	}

	// Tick AFTER the body's pose is evaluated, so the hard head-bound camera reads
	// the head bone's CURRENT position each frame (no one-frame clip on fast runs).
	if (GetMesh())
	{
		AddTickPrerequisiteComponent(GetMesh());
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		// Clamp pitch so the player can never flip the camera into their own back.
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = MinViewPitch;
			PC->PlayerCameraManager->ViewPitchMax = MaxViewPitch;
		}
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Owner visibility: the controlling player must always see the full body.
		MeshComp->SetOwnerNoSee(false);
		MeshComp->SetOnlyOwnerSee(false);
		MeshComp->SetVisibility(true, true);
		MeshComp->SetHiddenInGame(false, true);
		MeshComp->SetCastShadow(true);

		// The FBX was imported without materials; null slots render invisible on
		// skinned meshes, so force the engine default material everywhere.
		if (MeshComp->GetSkeletalMeshAsset())
		{
			UMaterialInterface* Skel = UMaterial::GetDefaultMaterial(MD_Surface);
			const int32 NumMats = MeshComp->GetNumMaterials();
			for (int32 i = 0; i < NumMats; ++i)
			{
				MeshComp->SetMaterial(i, Skel);
			}
		}

		// Auto-normalize the body to capsule size, whatever the FBX import scale was.
		if (USkeletalMesh* SK = MeshComp->GetSkeletalMeshAsset())
		{
			const FBoxSphereBounds B = SK->GetBounds();
			const float MeshHeight = FMath::Max(B.BoxExtent.Z * 2.f, 1.f);
			const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const float TargetHeight = HalfHeight * 2.f;

			const float ScaleFactor = TargetHeight / MeshHeight;
			MeshComp->SetWorldScale3D(FVector(ScaleFactor));

			const float ScaledBottom = (B.Origin.Z - B.BoxExtent.Z) * ScaleFactor;
			MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight - ScaledBottom));
			MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		}

		// Placed actors keep serialized component values, so re-apply anti-cull
		// settings at runtime too.
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		MeshComp->bEnableUpdateRateOptimizations = false;
		MeshComp->SetBoundsScale(4.f);
		MeshComp->bComponentUseFixedSkelBounds = true;

		SetupBodyAnimation();

		UE_LOG(LogTemp, Log, TEXT("BODYCAM: Mesh=%s AnimBP=%d FallbackAnim=%d"),
			MeshComp->GetSkeletalMeshAsset() ? *MeshComp->GetSkeletalMeshAsset()->GetName() : TEXT("<none>"),
			bUsingAnimBP ? 1 : 0, bAnimReady ? 1 : 0);
	}

	ApplyViewMode();
}

void ABodycamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Move);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABodycamCharacter::Look);

		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ABodycamCharacter::StartSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABodycamCharacter::StopSprint);

		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABodycamCharacter::StartCrouch);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABodycamCharacter::StopCrouch);

		EIC->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ABodycamCharacter::ToggleView);
	}
}

void ABodycamCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		AddMovementInput(Forward, Axis.Y);
		AddMovementInput(Right, Axis.X);
	}
}

void ABodycamCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ABodycamCharacter::StartSprint()
{
	bIsSprinting = true;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = SprintSpeed;
	}
}

void ABodycamCharacter::StopSprint()
{
	bIsSprinting = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}
}

void ABodycamCharacter::StartCrouch()
{
	Crouch();
}

void ABodycamCharacter::StopCrouch()
{
	UnCrouch();
}

void ABodycamCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (CameraSystem)
	{
		CameraSystem->NotifyLanded(FMath::Abs(PrevZVelocity) * LandingKickScale);
	}
}

void ABodycamCharacter::UpdateAnimInstanceVars(float Speed2D, float Dt)
{
	if (!bUsingAnimBP)
	{
		return;
	}

	if (!BodyAnim && GetMesh())
	{
		BodyAnim = GetMesh()->GetAnimInstance();
	}

	// Asymmetric smoothing: ease in when accelerating, but drop instantly on stop
	// so the walk anim doesn't keep playing after movement ends.
	SmoothedSpeed = (Speed2D < SmoothedSpeed)
		? Speed2D
		: FMath::FInterpTo(SmoothedSpeed, Speed2D, Dt, 10.f);

	const FRotator ControlRot = GetControlRotation();
	const float TargetPitch = FMath::Clamp(FRotator::NormalizeAxis(ControlRot.Pitch), MinViewPitch, MaxViewPitch);
	// In first person the body already yaws with the camera, so the head only
	// needs extra yaw in third person where the body faces its velocity.
	float TargetYaw = 0.f;
	if (bThirdPerson)
	{
		TargetYaw = FMath::Clamp(
			FRotator::NormalizeAxis(ControlRot.Yaw - GetActorRotation().Yaw), -70.f, 70.f);
	}

	SmoothedAimPitch = FMath::FInterpTo(SmoothedAimPitch, TargetPitch, Dt, 10.f);
	SmoothedAimYaw = FMath::FInterpTo(SmoothedAimYaw, TargetYaw, Dt, 10.f);

	// Feet match ground speed; idle keeps rate 1 so the blend-out stays natural.
	const float PlayRate = (SmoothedSpeed > 15.f)
		? FMath::Clamp(SmoothedSpeed / WalkSpeed, 0.7f, 1.6f)
		: 1.f;

	SetAnimFloat(BodyAnim, TEXT("Speed"), SmoothedSpeed);
	SetAnimFloat(BodyAnim, TEXT("AimPitch"), SmoothedAimPitch * AimPitchSign);
	SetAnimFloat(BodyAnim, TEXT("AimYaw"), SmoothedAimYaw * AimYawSign);
	SetAnimFloat(BodyAnim, TEXT("WalkPlayRate"), PlayRate);
}

void ABodycamCharacter::UpdateBodycamEffects(float Speed2D, float Dt)
{
	if (!FollowCamera || !CameraSystem)
	{
		return;
	}

	// The camera system owns ALL first-person motion + FOV. It eases itself out
	// (and zeroes its offsets) when third person disables it for the frame.
	FBodycamFrameState Frame;
	Frame.Speed2D = Speed2D;
	Frame.Velocity = GetVelocity();
	Frame.ControlRotation = GetControlRotation();
	Frame.bGrounded = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	Frame.bSprinting = bIsSprinting;
	Frame.bCrouched = bIsCrouched;
	Frame.bEnabled = !bThirdPerson;
	Frame.Dt = Dt;
	CameraSystem->UpdateSystem(Frame);
}

void ABodycamCharacter::UpdateBodyLean(float Dt)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || Dt <= 0.f)
	{
		return;
	}

	// Capture the auto-fit base rotation once; lean is composed on top of it
	// every frame, so the mesh always returns to exactly that pose at rest.
	if (!bBodyLeanSeeded)
	{
		bBodyLeanSeeded = true;
		BodyBaseRotation = MeshComp->GetRelativeRotation().Quaternion();
		PrevBodyVelocity = GetVelocity();
		LastBodyYaw = GetActorRotation().Yaw;
	}

	if (!bEnableBodyLean)
	{
		MeshComp->SetRelativeRotation(BodyBaseRotation);
		return;
	}

	const FVector Vel = GetVelocity();
	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const float TopSpeed = FMath::Max(SprintSpeed, 1.f);

	// Velocity-based lean: how the body carries itself while moving.
	const float LatRatio = FMath::Clamp(FVector::DotProduct(Vel, Right) / TopSpeed, -1.f, 1.f);
	const float FwdRatio = FMath::Clamp(FVector::DotProduct(Vel, Fwd) / TopSpeed, -1.f, 1.f);

	// Acceleration-based lean: the body anticipates starts and braces stops.
	// Differentiate and low-pass — raw per-frame deltas are too spiky for springs.
	const FVector Accel = (Vel - PrevBodyVelocity) / Dt;
	PrevBodyVelocity = Vel;
	BodyAccelFiltered = FMath::VInterpTo(BodyAccelFiltered, Accel, Dt, 8.f);
	const float AccelDiv = FMath::Max(AccelForFullBodyLean, 1.f);
	const float LatAccel = FMath::Clamp(FVector::DotProduct(BodyAccelFiltered, Right) / AccelDiv, -1.f, 1.f);
	const float FwdAccel = FMath::Clamp(FVector::DotProduct(BodyAccelFiltered, Fwd) / AccelDiv, -1.f, 1.f);

	// Turn banking: cornering at speed rolls the body into the turn, scaled by
	// how fast we're actually moving (turning on the spot shouldn't tilt).
	const float YawRate = FRotator::NormalizeAxis(GetActorRotation().Yaw - LastBodyYaw) / Dt;
	LastBodyYaw = GetActorRotation().Yaw;
	BodyYawRateFiltered = FMath::FInterpTo(BodyYawRateFiltered, YawRate, Dt, 8.f);
	const float SpeedRatio = FMath::Clamp(FVector(Vel.X, Vel.Y, 0.f).Size() / TopSpeed, 0.f, 1.f);
	const float TurnBank = FMath::Clamp(BodyYawRateFiltered / FMath::Max(TurnBodyLeanAtYawRate, 1.f), -1.f, 1.f)
		* TurnBodyLean * SpeedRatio;

	// Positive roll = tilt right; forward lean = negative pitch (applied below).
	float RollTarget = LatRatio * StrafeBodyLean + LatAccel * AccelBodyLean + TurnBank;
	float PitchTarget = FwdRatio * RunBodyLean + FwdAccel * AccelBodyLean;

	const bool bGrounded = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	if (!bGrounded)
	{
		RollTarget *= AirLeanScale;
		PitchTarget *= AirLeanScale;
	}

	RollTarget = FMath::Clamp(RollTarget, -MaxBodyLean, MaxBodyLean);
	PitchTarget = FMath::Clamp(PitchTarget, -MaxBodyLean, MaxBodyLean);

	BodyRollSpring.Update(RollTarget, BodyLeanFrequency, BodyLeanDamping, FMath::Min(Dt, 0.05f));
	BodyPitchSpring.Update(PitchTarget, BodyLeanFrequency, BodyLeanDamping, FMath::Min(Dt, 0.05f));

	// Lean lives in ACTOR space (pitch about right axis, roll about forward axis)
	// and composes onto the captured base rotation, so the mesh's own -90 yaw
	// never skews the axes. The mesh pivot sits at the feet — the body tilts
	// from the ankles, exactly how a person leans.
	const FQuat LeanQ(FRotator(-BodyPitchSpring.Value, 0.f, BodyRollSpring.Value));
	MeshComp->SetRelativeRotation(LeanQ * BodyBaseRotation);

	// Expose to the AnimBP too: future spine Transform Modify Bone nodes can
	// distribute the same lean across vertebrae instead of (or on top of) the
	// whole-mesh tilt.
	SetAnimFloat(BodyAnim, TEXT("LeanRoll"), BodyRollSpring.Value);
	SetAnimFloat(BodyAnim, TEXT("LeanForward"), BodyPitchSpring.Value);
}

void ABodycamCharacter::UpdateLocomotionFallback(float Speed2D, bool bGrounded)
{
	if (!bAnimReady)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const bool bWantWalk = bGrounded && Speed2D > 10.f;
	UAnimSequence* Desired = bWantWalk ? WalkAnim : IdleAnim;
	if (!Desired)
	{
		Desired = WalkAnim ? WalkAnim : IdleAnim;
	}

	if (Desired && MeshComp->GetSingleNodeInstance() &&
		MeshComp->GetSingleNodeInstance()->GetAnimationAsset() != Desired)
	{
		MeshComp->PlayAnimation(Desired, /*bLooping*/ true);
	}
	else if (Desired && !MeshComp->IsPlaying())
	{
		MeshComp->Play(true);
	}
}

void ABodycamCharacter::Tick(float Dt)
{
	Super::Tick(Dt);

	const FVector Vel = GetVelocity();
	const float Speed2D = FVector(Vel.X, Vel.Y, 0.f).Size();
	const bool bGrounded = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();

	if (bUsingAnimBP)
	{
		UpdateAnimInstanceVars(Speed2D, Dt);
	}
	else
	{
		UpdateLocomotionFallback(Speed2D, bGrounded);
	}

	UpdateBodyLean(Dt);
	// Position the lens AFTER the body pose/lean is final this frame.
	UpdateFirstPersonCamera(Dt);
	UpdateBodycamEffects(Speed2D, Dt);

	PrevZVelocity = Vel.Z;
}
