// First-person bodycam character: head-mounted camera, full body awareness,
// AnimBP-driven locomotion with smooth blends and head aim.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BodycamCameraSystem.h" // FBodycamSpring1D for the body lean
#include "BodycamCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimInstance;
class UBodycamCameraSystem;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class TEST_API ABodycamCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABodycamCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

protected:
	/** Boom that carries the camera. In first person it rides the head bone. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	/** Procedural bodycam motion (SCP:CB / VotV style): bob, breathing, trauma
	 *  shake, impacts, lean, aim inertia, FOV — all springs, zero positional lag. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UBodycamCameraSystem* CameraSystem;

	/** Hidden full-body twin that casts the shadow so hiding the view head never
	 *  produces a headless shadow. Follows the body via leader-pose. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USkeletalMeshComponent* ShadowMesh;

	/** Animation blueprint that drives the body (state machine + head aim). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TSubclassOf<UAnimInstance> BodyAnimClass;

	/** Fallback clips used only if the AnimBP could not be loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	class UAnimSequence* WalkAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	class UAnimSequence* IdleAnim = nullptr;

	// ---- Enhanced Input (built in C++, no .uasset needed) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* CrouchAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ToggleViewAction;

	// ---- Camera tuning ----
	UPROPERTY(EditAnywhere, Category = "Camera") float CameraDistance = 300.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float CameraHeight = 60.f;
	/** First-person eye height above the capsule centre (≈ eye-bone world height). */
	UPROPERTY(EditAnywhere, Category = "Camera") float FirstPersonEyeHeight = 68.f;
	/** Forward lens push from the head, in yaw-only space, so the lens stays ahead
	 *  of the body even when a run lunges the torso forward. */
	UPROPERTY(EditAnywhere, Category = "Camera") float FirstPersonEyeForward = 20.f;
	/** Lens height above the head bone (head bone ≈ jaw; eyes sit a little higher). */
	UPROPERTY(EditAnywhere, Category = "Camera") float FirstPersonEyeUpFromHead = 6.f;
	/** How fast the lens tracks the head position. High = glued (no lag); low = floaty. */
	UPROPERTY(EditAnywhere, Category = "Camera") float FirstPersonFollowSpeed = 30.f;
	/** Head bone (hidden from view in first person; full in shadow + 3rd person). */
	UPROPERTY(EditAnywhere, Category = "Camera") FName HeadBoneName = "Head";
	UPROPERTY(EditAnywhere, Category = "Camera") float BaseFOV = 100.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float SprintFOV = 112.f;
	/** Start in first person (bodycam). Toggle at runtime with V. */
	UPROPERTY(EditAnywhere, Category = "Camera") bool bThirdPerson = false;
	UPROPERTY(EditAnywhere, Category = "Camera") float MinViewPitch = -70.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float MaxViewPitch = 80.f;

	// ---- Bodycam feel ----
	// All motion tuning (bob, breathing, lean, shake, …) lives on the
	// CameraSystem component; only the mounting itself is configured here.
	/** Camera offset from the eye point, in view space (X fwd, Z up). Keeps the face out of frame. */
	UPROPERTY(EditAnywhere, Category = "Bodycam") FVector CameraFaceOffset = FVector(18.f, 0.f, 2.f);
	/** Scales the landing camera dip. */
	UPROPERTY(EditAnywhere, Category = "Bodycam") float LandingKickScale = 1.f;

	// ---- Procedural body lean (animation-independent) ----
	// The whole skeleton tilts from the ankles to match how the character moves:
	// banking into strafes and turns, leaning into acceleration. Works on top of
	// ANY animation — current or future clips need zero changes.
	UPROPERTY(EditAnywhere, Category = "Body Lean") bool bEnableBodyLean = true;
	/** Sideways roll (deg) at full-speed strafe. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float StrafeBodyLean = 7.f;
	/** Forward lean (deg) at full sprint speed. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float RunBodyLean = 4.f;
	/** Extra lean (deg) at full acceleration/braking — the body anticipates. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float AccelBodyLean = 4.5f;
	/** Acceleration (cm/s^2) that produces the full AccelBodyLean. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float AccelForFullBodyLean = 1400.f;
	/** Banking roll (deg) into a turn while moving (like a runner cornering). */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float TurnBodyLean = 5.f;
	/** Actor yaw rate (deg/s) that produces the full TurnBodyLean. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float TurnBodyLeanAtYawRate = 240.f;
	/** Hard cap (deg) on the combined lean per axis. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float MaxBodyLean = 12.f;
	/** Lean multiplier while airborne. */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float AirLeanScale = 0.4f;
	/** Lean spring frequency / damping (slightly under-damped = organic settle). */
	UPROPERTY(EditAnywhere, Category = "Body Lean") float BodyLeanFrequency = 2.4f;
	UPROPERTY(EditAnywhere, Category = "Body Lean") float BodyLeanDamping = 0.85f;

	// ---- Head aim tuning (sign flips in case the rig axes differ) ----
	UPROPERTY(EditAnywhere, Category = "Animation") float AimPitchSign = -1.f;
	UPROPERTY(EditAnywhere, Category = "Animation") float AimYawSign = 1.f;

	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 320.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float SprintSpeed = 620.f;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void ToggleView();

private:
	/** Binds the AnimBP (or the single-node fallback) to the mesh. */
	void SetupBodyAnimation();
	/** Pushes Speed / AimPitch / AimYaw / WalkPlayRate into the AnimBP every frame. */
	void UpdateAnimInstanceVars(float Speed2D, float DeltaSeconds);
	/** Procedural bodycam sway: lean, handheld noise, landing dip, FOV. */
	void UpdateBodycamEffects(float Speed2D, float DeltaSeconds);
	/** Tilts the whole skeleton to match movement (strafe bank, run lean, turn bank). */
	void UpdateBodyLean(float DeltaSeconds);
	/** Old single-node walk/idle swap, used only without the AnimBP. */
	void UpdateLocomotionFallback(float Speed2D, bool bGrounded);
	/** Applies first/third person camera + body rotation policy. */
	void ApplyViewMode();

	/** Drives the first-person lens to follow the head bone (translation only) so
	 *  the body can never overtake the camera on a run. Rotation stays on control. */
	void UpdateFirstPersonCamera(float Dt);
	FVector FPCamPos = FVector::ZeroVector;
	bool bFPCamSeeded = false;

	UPROPERTY(Transient)
	UAnimInstance* BodyAnim = nullptr;

	/** Anim instance of the hidden shadow twin, driven with the same vars as BodyAnim. */
	UPROPERTY(Transient)
	UAnimInstance* ShadowAnim = nullptr;

	bool bUsingAnimBP = false;
	bool bAnimReady = false;
	bool bIsSprinting = false;

	float SmoothedSpeed = 0.f;
	float SmoothedAimPitch = 0.f;
	float SmoothedAimYaw = 0.f;

	/** Vertical speed from the previous tick — Landed() needs the pre-impact value. */
	float PrevZVelocity = 0.f;

	// ---- body lean state ----
	FBodycamSpring1D BodyRollSpring;
	FBodycamSpring1D BodyPitchSpring;
	FVector PrevBodyVelocity = FVector::ZeroVector;
	FVector BodyAccelFiltered = FVector::ZeroVector;
	float LastBodyYaw = 0.f;
	float BodyYawRateFiltered = 0.f;
	/** Mesh relative rotation captured after BeginPlay's auto-fit — lean composes on top. */
	FQuat BodyBaseRotation = FQuat::Identity;
	bool bBodyLeanSeeded = false;
};
