// BodycamCameraSystem — procedural first-person camera motion in the spirit of
// SCP: Containment Breach and Voices of the Void.
//
// Design rules this module lives by:
//   * The camera NEVER trails the player positionally. All "weight" is rotational
//     or expressed as small local offsets driven by springs — the lens stays glued
//     to the head, so walking never leaves the camera behind.
//   * Everything is a spring or an oscillator. No raw Lerp-to-target: critically
//     damped springs give the organic overshoot/settle that makes handheld footage
//     read as "alive" instead of "interpolated".
//   * No colour grading. The image stays clean; the feel comes from motion alone.
//   * The system is fed, it never reaches into the owner. The character pushes an
//     FBodycamFrameState every frame; everything else is internal.
//
// Subsystems (each one isolated, individually tunable, individually toggleable),
// in fixed update order — earlier passes may feed later ones, never the reverse:
//   1. Fatigue        — sprinting builds exhaustion; exhaustion feeds breathing,
//                        sway and footstep weight. Recovers while resting.
//   2. Step cycle     — a phase oscillator locked to ground speed. Produces the
//                        figure-8 bob and fires a discrete impulse every footfall.
//   3. Breathing      — asymmetric inhale/exhale sway that fades in when idle and
//                        turns into audible-looking gasping when exhausted.
//   4. Idle drift     — the operator's head is never perfectly still: slow Perlin
//                        wander plus occasional random weight shifts foot-to-foot.
//   5. Accel sway     — starting, stopping and strafing tug the camera with the
//                        torso's acceleration: pitch on accel/brake, roll on
//                        lateral push. This is what makes walking feel attached.
//   6. Airborne       — jump apex weightlessness, then a growing fall rumble and
//                        nose-down drift the longer you drop.
//   7. Crouch         — posture pitch while crouched plus dip/bump transitions.
//   8. Trauma shake   — Perlin-noise shake driven by a decaying trauma scalar
//                        (AddTrauma API: explosions, scares, damage).
//   9. Impact springs — landing dips and footstep micro-thumps on a shared
//                        vertical spring with a pitch echo.
//  10. Lean           — strafe roll plus turn-rate roll, spring smoothed.
//  11. Aim inertia    — the head has mass: a small rotational spring lets the
//                        view drag a few degrees behind fast mouse flicks and
//                        settle with a touch of overshoot.
//  12. Impulses       — a free rotational spring game code can kick from any
//                        direction (AddCameraImpulse: hits, recoil, door slams).
//  13. FOV rig        — base/sprint FOV plus stackable temporary kicks.
//  14. Sensor noise   — sub-degree high-frequency jitter, the "cheap camera
//                        module bolted to a vest" signature.
//  15. Compose        — single write point; sums every contribution exactly once.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamCameraSystem.generated.h"

class UCameraComponent;

/** Critically-dampable 1D spring. Stable for any dt via semi-implicit Euler. */
USTRUCT()
struct FBodycamSpring1D
{
	GENERATED_BODY()

	float Value = 0.f;
	float Velocity = 0.f;

	/** Frequency in Hz-ish stiffness terms; Damping 1.0 = critical. */
	void Update(float Target, float Frequency, float Damping, float Dt)
	{
		const float Omega = 2.f * PI * Frequency;
		const float Accel = Omega * Omega * (Target - Value) - 2.f * Damping * Omega * Velocity;
		Velocity += Accel * Dt;
		Value += Velocity * Dt;
	}

	void Kick(float Impulse) { Velocity += Impulse; }
	void Reset(float V = 0.f) { Value = V; Velocity = 0.f; }
};

/** 3-axis rotational spring built from three 1D springs. */
USTRUCT()
struct FBodycamSpringRot
{
	GENERATED_BODY()

	FBodycamSpring1D Pitch;
	FBodycamSpring1D Yaw;
	FBodycamSpring1D Roll;

	void Update(const FRotator& Target, float Frequency, float Damping, float Dt)
	{
		Pitch.Update(Target.Pitch, Frequency, Damping, Dt);
		Yaw.Update(Target.Yaw, Frequency, Damping, Dt);
		Roll.Update(Target.Roll, Frequency, Damping, Dt);
	}

	void Kick(const FRotator& Impulse)
	{
		Pitch.Kick(Impulse.Pitch);
		Yaw.Kick(Impulse.Yaw);
		Roll.Kick(Impulse.Roll);
	}

	FRotator Get() const { return FRotator(Pitch.Value, Yaw.Value, Roll.Value); }
	void Reset() { Pitch.Reset(); Yaw.Reset(); Roll.Reset(); }
};

/**
 * Everything the camera system needs to know about the world this frame.
 * The character fills this in; the system never queries the owner directly
 * (apart from its right vector for lateral projections).
 */
struct FBodycamFrameState
{
	float Speed2D = 0.f;                       // ground speed, cm/s
	FVector Velocity = FVector::ZeroVector;    // full 3D velocity, cm/s
	FRotator ControlRotation = FRotator::ZeroRotator;
	bool bGrounded = true;
	bool bSprinting = false;
	bool bCrouched = false;
	bool bEnabled = true;                      // false while in third person
	float Dt = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBodycamFootstep, float, Intensity);

UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class TEST_API UBodycamCameraSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamCameraSystem();

	/** Wire the camera this system owns. Call once (BeginPlay). */
	void Init(UCameraComponent* InCamera);

	/** Per-frame state push from the character, followed by the full update. */
	void UpdateSystem(const FBodycamFrameState& Frame);

	// ---------------- public reactions ----------------

	/** 0..1 trauma; stacks and clamps. Shake amplitude = trauma^2. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddTrauma(float Amount);

	/** Landing notification; FallSpeed is the (positive) impact speed in cm/s. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void NotifyLanded(float FallSpeed);

	/** Temporary additive FOV kick that decays on a spring (sprint start, hits). */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddFOVKick(float Degrees);

	/**
	 * Kick the free impulse spring with a rotational velocity (deg/s per axis).
	 * Use for directional damage, recoil, a door slamming next to the player —
	 * anything that should shove the head and let it wobble back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddCameraImpulse(FRotator ImpulseDegPerSec);

	/** Current exhaustion 0..1 — exposed so gameplay/audio can key off it. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	float GetFatigue() const { return Fatigue; }

	/** Current trauma 0..1. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	float GetTrauma() const { return Trauma; }

	/** Fired every footfall while walking — hook footstep audio here. */
	UPROPERTY(BlueprintAssignable, Category = "Bodycam")
	FBodycamFootstep OnFootstep;

	// ---------------- tuning: master ----------------

	/** Scales every motion output (not FOV). 0 = static tripod, 1 = default. */
	UPROPERTY(EditAnywhere, Category = "Bodycam", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float GlobalIntensity = 1.f;

	// ---------------- tuning: fatigue ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") bool bEnableFatigue = true;
	/** Seconds of continuous sprint to reach full exhaustion. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float SprintSecondsToExhaust = 14.f;
	/** Seconds of rest to recover from full exhaustion to fresh. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float RecoverySeconds = 22.f;
	/** Breath rate multiplier added at full fatigue (1 -> rate * (1 + this)). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float FatigueBreathRateBonus = 1.1f;
	/** Breath amplitude multiplier added at full fatigue. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float FatigueBreathAmpBonus = 2.2f;
	/** How much breathing stays visible while MOVING at full fatigue (0..1). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float FatigueBreathFloor = 0.55f;
	/** Extra footstep impulse scale at full fatigue (heavy, sloppy steps). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Fatigue") float FatigueStepBonus = 0.35f;

	// ---------------- tuning: step bob ----------------

	/** Master switch for the walk bob. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") bool bEnableStepBob = true;
	/** Steps per second at reference walk speed. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float StepRateAtWalk = 1.9f;
	/** Speed (cm/s) the step rate is calibrated against. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float ReferenceWalkSpeed = 320.f;
	/** Vertical bob amplitude (cm) at full walk speed. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float BobVertical = 1.6f;
	/** Lateral figure-8 amplitude (cm). Half the vertical frequency. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float BobLateral = 1.1f;
	/** Roll wobble (deg) synced to the lateral sway. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float BobRoll = 0.7f;
	/** Pitch wobble (deg) synced to the vertical bob. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float BobPitch = 0.45f;
	/** Sprint multiplies bob amplitude by this. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float SprintBobScale = 1.45f;
	/** Crouch multiplies bob amplitude by this (careful, compact steps). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float CrouchBobScale = 0.55f;
	/** Downward velocity impulse (cm/s) fed to the impact spring per footfall. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Step") float StepImpulse = 6.5f;

	// ---------------- tuning: breathing ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") bool bEnableBreathing = true;
	/** Breaths per second when calm (0.25 = one full breath each 4s). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") float BreathRate = 0.27f;
	/** Vertical breathing travel (cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") float BreathVertical = 0.55f;
	/** Breathing pitch sway (deg). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") float BreathPitch = 0.30f;
	/** Breathing roll sway (deg) — shoulders rise unevenly. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") float BreathRoll = 0.12f;
	/** Speed (cm/s) above which breathing is fully suppressed by the step bob. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Breath") float BreathFadeOutSpeed = 90.f;

	// ---------------- tuning: idle drift ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") bool bEnableIdleDrift = true;
	/** Slow Perlin head wander amplitude (deg) when standing still. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float DriftDegrees = 0.8f;
	/** Wander frequency (Hz-ish); keep well under 0.5 or it reads as shake. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float DriftFrequency = 0.09f;
	/** Random weight-shift roll target range (deg). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float WeightShiftRoll = 1.4f;
	/** Random weight-shift lateral offset range (cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float WeightShiftOffset = 1.2f;
	/** Seconds between weight shifts (min/max, randomized each time). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float WeightShiftIntervalMin = 5.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float WeightShiftIntervalMax = 12.f;
	/** Weight-shift spring frequency — very soft, a lazy lean. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Idle") float WeightShiftFrequency = 0.35f;

	// ---------------- tuning: acceleration sway ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") bool bEnableAccelSway = true;
	/** Acceleration (cm/s^2) that produces the full sway. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float AccelForFullSway = 1400.f;
	/** Pitch (deg) at full forward acceleration (head tips back). Kept small so
	 *  starting a sprint doesn't read as the camera lagging behind. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float AccelPitch = 0.5f;
	/** Pitch (deg) at full braking (head nods forward). Sign handled internally. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float BrakePitch = 2.2f;
	/** Roll (deg) at full lateral acceleration. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float AccelRoll = 1.3f;
	/** Sway spring frequency / damping. Snappy + critically damped so the sway
	 *  settles immediately instead of trailing the body on run starts. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float AccelFrequency = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Accel") float AccelDamping = 1.0f;

	// ---------------- tuning: airborne ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") bool bEnableAirborne = true;
	/** Upward lift (cm) at the weightless jump apex. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float ApexLift = 1.6f;
	/** |Vz| (cm/s) below which the apex weightlessness is felt. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float ApexVelocityWindow = 220.f;
	/** Nose-down pitch (deg) reached at FallSpeedForFullEffect. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float FallPitch = 2.6f;
	/** Wind rumble amplitude (deg) reached at FallSpeedForFullEffect. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float FallRumbleDegrees = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float FallRumbleFrequency = 14.f;
	/** Fall speed (cm/s) where pitch and rumble reach their maxima. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Air") float FallSpeedForFullEffect = 1200.f;

	// ---------------- tuning: crouch ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Crouch") bool bEnableCrouch = true;
	/** Forward posture pitch (deg) while crouched — hunched, guarded. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Crouch") float CrouchPosturePitch = -1.4f;
	/** Impact-spring kick (cm/s) when dropping into the crouch. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Crouch") float CrouchDipImpulse = 26.f;
	/** Impact-spring kick (cm/s) when standing back up (smaller, upward). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Crouch") float StandBumpImpulse = 14.f;

	// ---------------- tuning: trauma shake ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Shake") bool bEnableTraumaShake = true;
	/** Trauma lost per second. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Shake") float TraumaDecay = 0.9f;
	/** Max rotational shake (deg per axis) at trauma = 1. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Shake") float ShakeMaxDegrees = 6.f;
	/** Max positional shake (cm) at trauma = 1. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Shake") float ShakeMaxOffset = 2.2f;
	/** Noise frequency for the shake (Hz-ish). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Shake") float ShakeFrequency = 11.f;

	// ---------------- tuning: impacts ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") bool bEnableImpacts = true;
	/** Landing dip (cm) per 100 cm/s of fall speed. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float LandDipPer100 = 0.55f;
	/** Hard cap on the landing dip depth (cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float LandDipMax = 9.f;
	/** Impact spring frequency / damping. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float ImpactFrequency = 4.2f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float ImpactDamping = 0.55f;
	/** How much of the vertical impact leaks into camera pitch (deg per cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float ImpactPitchPerCm = 0.65f;
	/** Landing also adds this much trauma per 100 cm/s of fall speed. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impact") float LandTraumaPer100 = 0.045f;

	// ---------------- tuning: lean ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") bool bEnableLean = true;
	/** Roll (deg) at full-speed strafe. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float StrafeRoll = 3.5f;
	/** Roll (deg) injected by fast turning. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float TurnRoll = 2.4f;
	/** Yaw rate (deg/s) that produces the full TurnRoll. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float TurnRollAtYawRate = 240.f;
	/** Lean spring frequency / damping. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanFrequency = 2.6f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanDamping = 0.9f;

	// ---------------- tuning: aim inertia ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|Inertia") bool bEnableAimInertia = true;
	/** Max degrees the view may trail behind a fast flick. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Inertia") float InertiaMaxLag = 3.5f;
	/** Degrees of lag produced per 100 deg/s of view rotation. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Inertia") float InertiaLagPer100 = 0.85f;
	/** Inertia spring frequency / damping (under-damped = slight overshoot). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Inertia") float InertiaFrequency = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Inertia") float InertiaDamping = 0.62f;

	// ---------------- tuning: impulses ----------------

	/** Free impulse spring frequency / damping (wobbles back after a shove). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impulse") float ImpulseFrequency = 4.5f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Impulse") float ImpulseDamping = 0.5f;

	// ---------------- tuning: FOV ----------------

	UPROPERTY(EditAnywhere, Category = "Bodycam|FOV") float BaseFOV = 100.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|FOV") float SprintFOV = 110.f;
	/** Sprint must exceed this ground speed before the FOV widens. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|FOV") float SprintFOVMinSpeed = 60.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|FOV") float FOVFrequency = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|FOV") float FOVDamping = 1.0f;

	// ---------------- tuning: sensor noise ----------------

	/** High-frequency jitter, off by default — it reads as tremor, not texture.
	 *  All the life the camera needs already comes from bob/breath/sway. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Noise") bool bEnableSensorNoise = false;
	/** Constant sub-degree jitter (deg) — the cheap-camera signature. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Noise") float SensorNoiseDegrees = 0.04f;
	/** Additional jitter (deg) blended in at full walk speed. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Noise") float SensorNoiseMoveBonus = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Noise") float SensorNoiseFrequency = 23.f;

protected:
	virtual void BeginPlay() override;

private:
	// ---- subsystem updates (called in order from UpdateSystem) ----
	void UpdateFatigue(const FBodycamFrameState& F);
	void UpdateStepCycle(const FBodycamFrameState& F);
	void UpdateBreathing(const FBodycamFrameState& F);
	void UpdateIdleDrift(const FBodycamFrameState& F);
	void UpdateAccelSway(const FBodycamFrameState& F);
	void UpdateAirborne(const FBodycamFrameState& F);
	void UpdateCrouch(const FBodycamFrameState& F);
	void UpdateTraumaShake(float Dt);
	void UpdateImpacts(float Dt);
	void UpdateLean(const FBodycamFrameState& F);
	void UpdateAimInertia(const FBodycamFrameState& F);
	void UpdateImpulses(float Dt);
	void UpdateFOV(const FBodycamFrameState& F);
	void UpdateSensorNoise(const FBodycamFrameState& F);
	void Compose();

	/** Smooth fade used when toggling to third person — motion eases out, not snaps. */
	float SystemWeight = 0.f;

	/** First UpdateSystem call seeds rate trackers so frame 0 doesn't spike. */
	bool bSeeded = false;

	UPROPERTY(Transient)
	UCameraComponent* Camera = nullptr;

	// ---- fatigue state ----
	float Fatigue = 0.f;            // 0 fresh .. 1 exhausted

	// ---- step cycle state ----
	float StepPhase = 0.f;          // radians; one footfall every PI
	float LastStepPhase = 0.f;
	float BobBlend = 0.f;           // 0 idle .. 1 moving
	float BobSpeedSmoothed = 0.f;   // smoothed speed ratio — raw speed snaps to 0 on stop and would jerk the bob
	FVector BobOffset = FVector::ZeroVector;
	FRotator BobRot = FRotator::ZeroRotator;

	// ---- breathing state ----
	float BreathPhase = 0.f;
	float BreathBlend = 0.f;
	FVector BreathOffset = FVector::ZeroVector;
	FRotator BreathRot = FRotator::ZeroRotator;

	// ---- idle drift state ----
	float IdleBlend = 0.f;          // 0 moving .. 1 settled
	float DriftTime = 0.f;
	FRotator DriftRot = FRotator::ZeroRotator;
	float WeightShiftTimer = 3.f;
	float WeightShiftRollTarget = 0.f;
	float WeightShiftOffsetTarget = 0.f;
	FBodycamSpring1D WeightRollSpring;
	FBodycamSpring1D WeightOffsetSpring;

	// ---- per-frame derivatives ----
	// Computed ONCE per frame against last frame's samples, then consumed by
	// the substepped passes below — recomputing them per substep would inflate
	// the rates by the substep count.
	FVector FrameAccel = FVector::ZeroVector;
	float FrameYawRate = 0.f;
	float FramePitchRate = 0.f;

	// ---- acceleration sway state ----
	FVector PrevVelocity = FVector::ZeroVector;
	FVector AccelFiltered = FVector::ZeroVector;
	FBodycamSpring1D AccelPitchSpring;
	FBodycamSpring1D AccelRollSpring;

	// ---- airborne state ----
	float AirBlend = 0.f;           // eases in/out around ground transitions
	float AirTime = 0.f;
	float AirFloatZ = 0.f;
	FRotator AirRot = FRotator::ZeroRotator;
	float AirRumbleTime = 0.f;

	// ---- crouch state ----
	bool bWasCrouched = false;
	float CrouchBlend = 0.f;

	// ---- trauma state ----
	float Trauma = 0.f;
	float ShakeTime = 0.f;
	FVector ShakeOffset = FVector::ZeroVector;
	FRotator ShakeRot = FRotator::ZeroRotator;

	// ---- impact spring ----
	FBodycamSpring1D ImpactZ;

	// ---- lean ----
	FBodycamSpring1D LeanRoll;
	float LastYaw = 0.f;
	float YawRateFiltered = 0.f;

	// ---- aim inertia ----
	FBodycamSpring1D InertiaPitch;
	FBodycamSpring1D InertiaYaw;
	float LastPitch = 0.f;
	float PitchRateFiltered = 0.f;

	// ---- free impulse spring ----
	FBodycamSpringRot ImpulseSpring;

	// ---- FOV ----
	FBodycamSpring1D FOVSpring;
	float FOVKick = 0.f;

	// ---- sensor noise ----
	float NoiseTime = 0.f;
	FRotator SensorRot = FRotator::ZeroRotator;

	// composed every frame
	FVector FinalOffset = FVector::ZeroVector;
	FRotator FinalRot = FRotator::ZeroRotator;
};
