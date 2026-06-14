// BodycamCameraSystem.cpp — see the header for the design contract.
//
// Update order matters and is fixed:
//   fatigue -> step -> breathing -> idle drift -> accel sway -> airborne ->
//   crouch -> trauma -> impacts -> lean -> inertia -> impulses -> fov ->
//   noise -> compose
// Earlier subsystems may feed later ones (fatigue scales breathing, footfalls
// kick the impact spring, landings add trauma), never the other way around.

#include "BodycamCameraSystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

UBodycamCameraSystem::UBodycamCameraSystem()
{
	PrimaryComponentTick.bCanEverTick = false; // driven explicitly by the owner
}

void UBodycamCameraSystem::BeginPlay()
{
	Super::BeginPlay();
	FOVSpring.Reset(BaseFOV);
}

void UBodycamCameraSystem::Init(UCameraComponent* InCamera)
{
	Camera = InCamera;
	if (Camera)
	{
		FOVSpring.Reset(Camera->FieldOfView);
	}
}

// ---------------------------------------------------------------------------
// Public reactions — small, dumb entry points. All the behaviour lives in the
// per-frame passes; these only deposit energy into the right accumulator.
// ---------------------------------------------------------------------------

void UBodycamCameraSystem::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

void UBodycamCameraSystem::NotifyLanded(float FallSpeed)
{
	if (!bEnableImpacts)
	{
		return;
	}
	const float Per100 = FMath::Max(FallSpeed, 0.f) / 100.f;
	const float Dip = FMath::Min(Per100 * LandDipPer100, LandDipMax);
	// A landing is a velocity event: kick the spring downward instead of
	// teleporting it, so depth and recovery both scale with the fall.
	ImpactZ.Kick(-Dip * ImpactFrequency * 6.f);
	AddTrauma(Per100 * LandTraumaPer100);
}

void UBodycamCameraSystem::AddFOVKick(float Degrees)
{
	FOVKick += Degrees;
}

void UBodycamCameraSystem::AddCameraImpulse(FRotator ImpulseDegPerSec)
{
	ImpulseSpring.Kick(ImpulseDegPerSec);
}

// ---------------------------------------------------------------------------
// Main update — fixed pipeline, one camera write at the very end.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateSystem(const FBodycamFrameState& Frame)
{
	if (!Camera || Frame.Dt <= 0.f)
	{
		return;
	}

	// A hitch (level load, alt-tab, shader compile) must not be integrated as
	// one giant step — clamp it and let the sim simply lose that time.
	const float Dt = FMath::Min(Frame.Dt, 0.25f);

	// Seed the rate trackers on the first real frame so the deltas computed
	// against "last frame" don't see garbage and whip the springs.
	if (!bSeeded)
	{
		bSeeded = true;
		LastYaw = Frame.ControlRotation.Yaw;
		LastPitch = Frame.ControlRotation.Pitch;
		PrevVelocity = Frame.Velocity;
		bWasCrouched = Frame.bCrouched;
	}

	// Ease the whole system in/out (third-person toggle) instead of snapping.
	const float WeightTarget = Frame.bEnabled ? 1.f : 0.f;
	SystemWeight = FMath::FInterpTo(SystemWeight, WeightTarget, Dt, 8.f);

	// Per-frame derivatives, computed once against last frame's samples.
	FrameAccel = (Frame.Velocity - PrevVelocity) / Dt;
	FrameYawRate = FRotator::NormalizeAxis(Frame.ControlRotation.Yaw - LastYaw) / Dt;
	FramePitchRate = FRotator::NormalizeAxis(Frame.ControlRotation.Pitch - LastPitch) / Dt;
	LastYaw = Frame.ControlRotation.Yaw;
	LastPitch = Frame.ControlRotation.Pitch;
	PrevVelocity = Frame.Velocity;

	// CRITICAL: substep the simulation. Semi-implicit Euler springs at 4-5 Hz
	// are only stable while Omega*dt stays small — fed a 30 fps (or hitchy)
	// delta they oscillate violently instead of settling. Substepping keeps
	// every integration step at ~1/90 s, so the feel is identical at any
	// framerate.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt * 90.f), 1, 8);
	FBodycamFrameState Sub = Frame;
	Sub.Dt = Dt / static_cast<float>(Steps);

	for (int32 StepIdx = 0; StepIdx < Steps; ++StepIdx)
	{
		UpdateFatigue(Sub);
		UpdateStepCycle(Sub);
		UpdateBreathing(Sub);
		UpdateIdleDrift(Sub);
		UpdateAccelSway(Sub);
		UpdateAirborne(Sub);
		UpdateCrouch(Sub);
		UpdateTraumaShake(Sub.Dt);
		UpdateImpacts(Sub.Dt);
		UpdateLean(Sub);
		UpdateAimInertia(Sub);
		UpdateImpulses(Sub.Dt);
		UpdateFOV(Sub);
		UpdateSensorNoise(Sub);
	}
	Compose();
}

// ---------------------------------------------------------------------------
// 1. Fatigue — the body remembers the sprint.
//
// A single 0..1 scalar that climbs while sprinting at speed and bleeds away at
// rest. It never moves the camera directly; it feeds breathing (rate, depth,
// and how much of it survives while moving) and footstep weight. The payoff is
// the VotV/SCP:CB loop: run from the thing, stop, and the camera keeps heaving
// for the next half-minute — the player FEELS the run they just survived.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateFatigue(const FBodycamFrameState& F)
{
	if (!bEnableFatigue)
	{
		Fatigue = 0.f;
		return;
	}

	const bool bExerting = F.bSprinting && F.Speed2D > ReferenceWalkSpeed * 0.8f && F.bGrounded;
	if (bExerting)
	{
		Fatigue += F.Dt / FMath::Max(SprintSecondsToExhaust, 1.f);
	}
	else
	{
		// Recovery slows down while still walking — you don't catch your
		// breath mid-march.
		const float RestQuality = 1.f - 0.6f * FMath::Clamp(F.Speed2D / FMath::Max(ReferenceWalkSpeed, 1.f), 0.f, 1.f);
		Fatigue -= (F.Dt / FMath::Max(RecoverySeconds, 1.f)) * RestQuality;
	}
	Fatigue = FMath::Clamp(Fatigue, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// 2. Step cycle — the metronome of the whole system.
//
// Phase advances with actual ground speed, so the bob frequency is always in
// sync with the legs: slow walk = lazy sway, sprint = urgent pounding. A
// footfall happens every PI radians; at that instant we kick the impact spring
// and broadcast OnFootstep so audio can key off the exact same beat the eye
// sees. Vertical bob runs at 2x phase (two foot plants per gait cycle), the
// lateral figure-8 at 1x — the classic handheld walk signature.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateStepCycle(const FBodycamFrameState& F)
{
	if (!bEnableStepBob)
	{
		BobOffset = FVector::ZeroVector;
		BobRot = FRotator::ZeroRotator;
		return;
	}

	const float SpeedRatio = FMath::Clamp(F.Speed2D / FMath::Max(ReferenceWalkSpeed, 1.f), 0.f, 2.f);
	const bool bMoving = F.bGrounded && F.Speed2D > 15.f;

	// Blend amplitude in/out smoothly so stopping doesn't freeze the bob mid-air.
	BobBlend = FMath::FInterpTo(BobBlend, bMoving ? 1.f : 0.f, F.Dt, bMoving ? 6.f : 10.f);
	// Smooth the speed ratio too: braking kills raw speed within a frame, and an
	// amplitude tied to it would yank the camera to zero instead of easing out.
	BobSpeedSmoothed = FMath::FInterpTo(BobSpeedSmoothed, SpeedRatio, F.Dt, 7.f);

	if (bMoving)
	{
		// Step frequency scales with the square root of speed — natural gait:
		// you lengthen your stride before you hurry it.
		const float StepsPerSec = StepRateAtWalk * FMath::Sqrt(FMath::Max(SpeedRatio, 0.05f));
		LastStepPhase = StepPhase;
		StepPhase += StepsPerSec * PI * F.Dt;

		// Footfall: every PI of phase.
		if (FMath::FloorToInt(StepPhase / PI) != FMath::FloorToInt(LastStepPhase / PI))
		{
			float Intensity = FMath::Clamp(SpeedRatio * (F.bSprinting ? 1.3f : 1.f), 0.2f, 2.f);
			// Exhausted steps land heavier — the legs stop absorbing them.
			Intensity *= 1.f + Fatigue * FatigueStepBonus;
			if (bEnableImpacts)
			{
				ImpactZ.Kick(-StepImpulse * Intensity);
			}
			OnFootstep.Broadcast(Intensity);
		}
	}

	float Amp = BobBlend * FMath::Sqrt(FMath::Max(BobSpeedSmoothed, 0.f))
		* (F.bSprinting ? SprintBobScale : 1.f);
	if (F.bCrouched)
	{
		Amp *= CrouchBobScale;
	}

	// Vertical at 2x (each plant), lateral/roll at 1x (weight shifts side to side).
	const float V = FMath::Sin(StepPhase * 2.f);
	const float L = FMath::Sin(StepPhase);

	BobOffset = FVector(0.f, L * BobLateral * Amp, -FMath::Abs(V) * BobVertical * Amp);
	BobRot = FRotator(
		V * BobPitch * Amp,
		0.f,
		L * BobRoll * Amp);
}

// ---------------------------------------------------------------------------
// 3. Breathing — only audible (visible) when you stand still… unless you're
// winded.
//
// Real breathing is asymmetric: a quicker inhale, a slower exhale, a beat of
// rest. We shape a sine into that with a power curve on the positive half.
// Fatigue pushes the rate up, deepens the travel, and — crucially — keeps a
// floor of breathing visible even while moving, so a gasping player can't hide
// it by walking.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateBreathing(const FBodycamFrameState& F)
{
	if (!bEnableBreathing)
	{
		BreathOffset = FVector::ZeroVector;
		BreathRot = FRotator::ZeroRotator;
		return;
	}

	const float Calm = 1.f - FMath::Clamp(F.Speed2D / FMath::Max(BreathFadeOutSpeed, 1.f), 0.f, 1.f);
	const float Target = FMath::Max(Calm, Fatigue * FatigueBreathFloor);
	BreathBlend = FMath::FInterpTo(BreathBlend, Target, F.Dt, 2.5f);

	const float Rate = BreathRate * (1.f + Fatigue * FatigueBreathRateBonus);
	const float AmpScale = 1.f + Fatigue * FatigueBreathAmpBonus;

	BreathPhase += Rate * 2.f * PI * F.Dt;
	const float S = FMath::Sin(BreathPhase);
	// inhale (S>0) sharpened, exhale (S<0) relaxed
	const float Shaped = (S >= 0.f)
		? FMath::Pow(S, 0.75f)
		: -FMath::Pow(-S, 1.35f);

	const float A = BreathBlend * AmpScale;
	BreathOffset = FVector(0.f, 0.f, Shaped * BreathVertical * A);
	BreathRot = FRotator(
		Shaped * BreathPitch * A,
		0.f,
		// Roll rides a quarter-phase behind the pitch — shoulders rise unevenly.
		FMath::Sin(BreathPhase - PI * 0.25f) * BreathRoll * A);
}

// ---------------------------------------------------------------------------
// 4. Idle drift — nobody stands like a statue.
//
// Two layers, both gated by an "idle blend" that takes a couple of seconds to
// settle in after the player stops:
//   a) A slow Perlin wander, sub-degree, far below shake frequency — the head
//      micro-corrects its balance constantly.
//   b) Discrete weight shifts: every 5–12 seconds the body leans onto the
//      other foot. A very soft spring eases toward a new random roll/lateral
//      offset and just… stays there, the way a bored guard actually stands.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateIdleDrift(const FBodycamFrameState& F)
{
	if (!bEnableIdleDrift)
	{
		DriftRot = FRotator::ZeroRotator;
		WeightRollSpring.Reset();
		WeightOffsetSpring.Reset();
		return;
	}

	const bool bSettled = F.bGrounded && F.Speed2D < 20.f;
	// Slow ease-in (you settle), fast ease-out (any movement breaks the pose).
	IdleBlend = FMath::FInterpTo(IdleBlend, bSettled ? 1.f : 0.f, F.Dt, bSettled ? 0.6f : 5.f);

	// a) Perlin wander.
	DriftTime += F.Dt * DriftFrequency * 2.f * PI;
	DriftRot = FRotator(
		FMath::PerlinNoise1D(DriftTime) * DriftDegrees * IdleBlend,
		FMath::PerlinNoise1D(DriftTime + 53.1f) * DriftDegrees * 1.2f * IdleBlend,
		FMath::PerlinNoise1D(DriftTime + 167.4f) * DriftDegrees * 0.6f * IdleBlend);

	// b) Weight shifts.
	if (bSettled)
	{
		WeightShiftTimer -= F.Dt;
		if (WeightShiftTimer <= 0.f)
		{
			WeightShiftTimer = FMath::FRandRange(WeightShiftIntervalMin, WeightShiftIntervalMax);
			// Lean the OTHER way more often than the same way again.
			const float Dir = (WeightShiftRollTarget > 0.f) ? -1.f : 1.f;
			WeightShiftRollTarget = Dir * FMath::FRandRange(0.4f, 1.f) * WeightShiftRoll;
			WeightShiftOffsetTarget = Dir * FMath::FRandRange(0.3f, 1.f) * WeightShiftOffset;
		}
	}
	else
	{
		// Movement recentres the stance and rewinds the timer a little, so a
		// brief shuffle doesn't immediately trigger a shift on re-settle.
		WeightShiftRollTarget = 0.f;
		WeightShiftOffsetTarget = 0.f;
		WeightShiftTimer = FMath::Max(WeightShiftTimer, 2.f);
	}

	WeightRollSpring.Update(WeightShiftRollTarget * IdleBlend, WeightShiftFrequency, 1.f, F.Dt);
	WeightOffsetSpring.Update(WeightShiftOffsetTarget * IdleBlend, WeightShiftFrequency, 1.f, F.Dt);
}

// ---------------------------------------------------------------------------
// 5. Acceleration sway — mass resists the legs.
//
// The torso is a weight on top of the hips: push off and the head tips back,
// brake and it nods forward (harder — braking is more violent than starting),
// strafe and it rolls into the push. We differentiate velocity ourselves,
// low-pass it (raw per-frame deltas are too spiky to feed a spring), project
// onto the owner's local axes and let two under-damped springs chase it.
// This pass is the single biggest reason walking feels ATTACHED rather than
// gliding — the camera reacts to every start, stop, and direction change.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateAccelSway(const FBodycamFrameState& F)
{
	if (!bEnableAccelSway)
	{
		AccelPitchSpring.Reset();
		AccelRollSpring.Reset();
		return;
	}

	// FrameAccel is differentiated once per frame in UpdateSystem; here we only
	// low-pass it (raw per-frame deltas are too spiky to feed a spring).
	AccelFiltered = FMath::VInterpTo(AccelFiltered, FrameAccel, F.Dt, 9.f);

	const AActor* Owner = GetOwner();
	const FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Right = Owner ? Owner->GetActorRightVector() : FVector::RightVector;

	const float Longitudinal = FVector::DotProduct(AccelFiltered, Fwd) / FMath::Max(AccelForFullSway, 1.f);
	const float Lateral = FVector::DotProduct(AccelFiltered, Right) / FMath::Max(AccelForFullSway, 1.f);

	// Asymmetric pitch: acceleration tips the head back gently, braking throws
	// it forward harder.
	const float L = FMath::Clamp(Longitudinal, -1.f, 1.f);
	const float PitchTarget = (L >= 0.f) ? L * AccelPitch : L * BrakePitch;
	// Lateral acceleration rolls the head INTO the push (counterweight).
	const float RollTarget = FMath::Clamp(Lateral, -1.f, 1.f) * AccelRoll;

	AccelPitchSpring.Update(PitchTarget, AccelFrequency, AccelDamping, F.Dt);
	AccelRollSpring.Update(RollTarget, AccelFrequency, AccelDamping, F.Dt);
}

// ---------------------------------------------------------------------------
// 6. Airborne — two distinct sensations, one pass.
//
// The apex: as vertical speed crosses zero the body goes weightless for a
// beat; the camera floats up a centimetre and everything else quiets down.
// The fall: speed builds, a wind rumble fades in and the view is dragged
// nose-down — the longer the drop, the louder the camera screams about it,
// which sets up the landing dip that NotifyLanded delivers on touchdown.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateAirborne(const FBodycamFrameState& F)
{
	if (!bEnableAirborne)
	{
		AirFloatZ = 0.f;
		AirRot = FRotator::ZeroRotator;
		return;
	}

	AirBlend = FMath::FInterpTo(AirBlend, F.bGrounded ? 0.f : 1.f, F.Dt, F.bGrounded ? 10.f : 5.f);
	AirTime = F.bGrounded ? 0.f : AirTime + F.Dt;

	if (AirBlend <= KINDA_SMALL_NUMBER)
	{
		AirFloatZ = 0.f;
		AirRot = FRotator::ZeroRotator;
		return;
	}

	const float Vz = F.Velocity.Z;

	// Weightless window around the apex (|Vz| small).
	const float Apex = 1.f - FMath::Clamp(FMath::Abs(Vz) / FMath::Max(ApexVelocityWindow, 1.f), 0.f, 1.f);
	AirFloatZ = Apex * ApexLift * AirBlend;

	// Fall build-up: only downward velocity counts.
	const float FallFactor = FMath::Clamp(-Vz / FMath::Max(FallSpeedForFullEffect, 1.f), 0.f, 1.f);
	// Square the rumble's growth so short hops stay quiet and real drops roar.
	const float Rumble = FallFactor * FallFactor * FallRumbleDegrees * AirBlend;

	AirRumbleTime += F.Dt * FallRumbleFrequency;
	AirRot = FRotator(
		-FallFactor * FallPitch * AirBlend + FMath::PerlinNoise1D(AirRumbleTime) * Rumble,
		FMath::PerlinNoise1D(AirRumbleTime + 61.7f) * Rumble * 0.7f,
		FMath::PerlinNoise1D(AirRumbleTime + 113.9f) * Rumble * 1.2f);
}

// ---------------------------------------------------------------------------
// 7. Crouch — posture, not just capsule height.
//
// The capsule resize already moves the camera down; what it doesn't give you
// is the BODY of the move. Dropping into a crouch kicks the shared impact
// spring (knees take the hit), standing back up bumps it the other way, and
// while crouched the camera holds a small forward posture pitch — hunched,
// guarded, exactly the SCP:CB sneak silhouette.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateCrouch(const FBodycamFrameState& F)
{
	if (!bEnableCrouch)
	{
		CrouchBlend = 0.f;
		bWasCrouched = F.bCrouched;
		return;
	}

	if (F.bCrouched != bWasCrouched)
	{
		if (bEnableImpacts)
		{
			ImpactZ.Kick(F.bCrouched ? -CrouchDipImpulse : StandBumpImpulse);
		}
		bWasCrouched = F.bCrouched;
	}

	CrouchBlend = FMath::FInterpTo(CrouchBlend, F.bCrouched ? 1.f : 0.f, F.Dt, 5.f);
}

// ---------------------------------------------------------------------------
// 8. Trauma shake — one scalar to rule every scare.
//
// Game code calls AddTrauma(); amplitude follows trauma^2 so small bumps barely
// register while real hits feel violent. Perlin noise on three uncorrelated
// channels keeps it organic (sines read as mechanical wobble immediately).
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateTraumaShake(float Dt)
{
	if (!bEnableTraumaShake)
	{
		ShakeOffset = FVector::ZeroVector;
		ShakeRot = FRotator::ZeroRotator;
		return;
	}

	Trauma = FMath::Max(Trauma - TraumaDecay * Dt, 0.f);
	if (Trauma <= KINDA_SMALL_NUMBER)
	{
		ShakeOffset = FVector::ZeroVector;
		ShakeRot = FRotator::ZeroRotator;
		return;
	}

	ShakeTime += Dt * ShakeFrequency;
	const float Power = Trauma * Trauma;

	ShakeRot = FRotator(
		FMath::PerlinNoise1D(ShakeTime) * ShakeMaxDegrees * Power,
		FMath::PerlinNoise1D(ShakeTime + 71.3f) * ShakeMaxDegrees * Power,
		FMath::PerlinNoise1D(ShakeTime + 139.7f) * ShakeMaxDegrees * Power * 1.25f);

	ShakeOffset = FVector(
		0.f,
		FMath::PerlinNoise1D(ShakeTime + 211.9f) * ShakeMaxOffset * Power,
		FMath::PerlinNoise1D(ShakeTime + 307.1f) * ShakeMaxOffset * Power);
}

// ---------------------------------------------------------------------------
// 9. Impact spring — landings, footfalls and crouches share one vertical spring.
//
// The spring is under-damped on purpose: a heavy landing dips, rebounds a
// little past neutral and settles — that rebound is what sells the weight.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateImpacts(float Dt)
{
	if (!bEnableImpacts)
	{
		ImpactZ.Reset();
		return;
	}
	ImpactZ.Update(0.f, ImpactFrequency, ImpactDamping, Dt);
	ImpactZ.Value = FMath::Clamp(ImpactZ.Value, -LandDipMax, LandDipMax * 0.4f);
}

// ---------------------------------------------------------------------------
// 10. Lean — the horizon reacts to lateral intent.
//
// Strafing banks the camera into the motion; whipping the mouse drags the
// horizon against the turn. Both ride one spring so they fight each other
// naturally when you strafe-turn.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateLean(const FBodycamFrameState& F)
{
	if (!bEnableLean)
	{
		LeanRoll.Reset();
		return;
	}

	const AActor* Owner = GetOwner();
	const FVector Right = Owner ? Owner->GetActorRightVector() : FVector::RightVector;
	const float Lateral = FMath::Clamp(
		FVector::DotProduct(F.Velocity, Right) / FMath::Max(ReferenceWalkSpeed * 2.f, 1.f), -1.f, 1.f);

	YawRateFiltered = FMath::FInterpTo(YawRateFiltered, FrameYawRate, F.Dt, 8.f);

	const float Target =
		Lateral * StrafeRoll
		+ FMath::Clamp(YawRateFiltered / FMath::Max(TurnRollAtYawRate, 1.f), -1.f, 1.f) * TurnRoll;

	LeanRoll.Update(Target, LeanFrequency, LeanDamping, F.Dt);
}

// ---------------------------------------------------------------------------
// 11. Aim inertia — the head is a mass, not a gimbal.
//
// We never lag the camera's real rotation (that would fight the player's hand).
// Instead we measure how fast the view is rotating and let a small additive
// offset trail in the opposite direction, returning with a touch of overshoot.
// Capped hard so combat aiming stays honest.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateAimInertia(const FBodycamFrameState& F)
{
	if (!bEnableAimInertia)
	{
		InertiaPitch.Reset();
		InertiaYaw.Reset();
		return;
	}

	PitchRateFiltered = FMath::FInterpTo(PitchRateFiltered, FramePitchRate, F.Dt, 10.f);

	// YawRateFiltered is produced by the lean pass right above this one.
	const float LagYawTarget = FMath::Clamp(
		-YawRateFiltered / 100.f * InertiaLagPer100, -InertiaMaxLag, InertiaMaxLag);
	const float LagPitchTarget = FMath::Clamp(
		-PitchRateFiltered / 100.f * InertiaLagPer100 * 0.7f, -InertiaMaxLag, InertiaMaxLag);

	InertiaYaw.Update(LagYawTarget, InertiaFrequency, InertiaDamping, F.Dt);
	InertiaPitch.Update(LagPitchTarget, InertiaFrequency, InertiaDamping, F.Dt);
}

// ---------------------------------------------------------------------------
// 12. Impulses — the free spring game code shoves around.
//
// AddCameraImpulse() deposits rotational velocity; here it just relaxes back
// to zero, under-damped so a hard hit visibly wobbles before settling. This
// is the hook for directional damage, weapon recoil, a door slamming shut an
// inch from the lens.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateImpulses(float Dt)
{
	ImpulseSpring.Update(FRotator::ZeroRotator, ImpulseFrequency, ImpulseDamping, Dt);
}

// ---------------------------------------------------------------------------
// 13. FOV — one spring carrying base/sprint plus decaying kicks.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateFOV(const FBodycamFrameState& F)
{
	FOVKick = FMath::FInterpTo(FOVKick, 0.f, F.Dt, 5.f);

	const float Target =
		((F.bSprinting && F.Speed2D > SprintFOVMinSpeed) ? SprintFOV : BaseFOV) + FOVKick;
	FOVSpring.Update(Target, FOVFrequency, FOVDamping, F.Dt);

	if (Camera)
	{
		Camera->SetFieldOfView(FOVSpring.Value);
	}
}

// ---------------------------------------------------------------------------
// 14. Sensor noise — the cheap-camera fingerprint.
//
// Sub-degree, high-frequency, always on. This is what separates "smooth game
// camera" from "device strapped to a person". Kept tiny so it never reads as
// shake, only as texture.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::UpdateSensorNoise(const FBodycamFrameState& F)
{
	if (!bEnableSensorNoise)
	{
		SensorRot = FRotator::ZeroRotator;
		return;
	}

	NoiseTime += F.Dt * SensorNoiseFrequency;
	const float MoveBlend = FMath::Clamp(F.Speed2D / FMath::Max(ReferenceWalkSpeed, 1.f), 0.f, 1.f);
	const float Amp = SensorNoiseDegrees + SensorNoiseMoveBonus * MoveBlend;

	SensorRot = FRotator(
		FMath::PerlinNoise1D(NoiseTime) * Amp,
		FMath::PerlinNoise1D(NoiseTime + 41.2f) * Amp * 0.8f,
		FMath::PerlinNoise1D(NoiseTime + 97.6f) * Amp * 1.15f);
}

// ---------------------------------------------------------------------------
// 15. Compose — sum every contribution and write it to the camera ONCE.
//
// Single write point = no subsystem can fight another over the transform, and
// the whole stack scales by SystemWeight (view-mode ease) times GlobalIntensity
// (designer master knob). FOV is intentionally outside the scaling — narrowing
// the motion shouldn't squash the lens.
// ---------------------------------------------------------------------------
void UBodycamCameraSystem::Compose()
{
	const float ImpactCm = ImpactZ.Value;

	FVector Offset =
		BobOffset
		+ BreathOffset
		+ ShakeOffset
		+ FVector(0.f, WeightOffsetSpring.Value, ImpactCm + AirFloatZ);

	FRotator Rot(
		// pitch
		BobRot.Pitch + BreathRot.Pitch + ShakeRot.Pitch
			+ ImpactCm * ImpactPitchPerCm * 0.5f
			+ InertiaPitch.Value
			+ AccelPitchSpring.Value
			+ AirRot.Pitch
			+ CrouchBlend * CrouchPosturePitch
			+ DriftRot.Pitch
			+ ImpulseSpring.Pitch.Value
			+ SensorRot.Pitch,
		// yaw
		BobRot.Yaw + ShakeRot.Yaw
			+ InertiaYaw.Value
			+ AirRot.Yaw
			+ DriftRot.Yaw
			+ ImpulseSpring.Yaw.Value
			+ SensorRot.Yaw,
		// roll
		BobRot.Roll + BreathRot.Roll + ShakeRot.Roll
			+ LeanRoll.Value
			+ AccelRollSpring.Value
			+ AirRot.Roll
			+ DriftRot.Roll + WeightRollSpring.Value
			+ ImpulseSpring.Roll.Value
			+ SensorRot.Roll);

	const float Scale = SystemWeight * FMath::Max(GlobalIntensity, 0.f);
	Offset *= Scale;
	Rot = FRotator(Rot.Pitch * Scale, Rot.Yaw * Scale, Rot.Roll * Scale);

	FinalOffset = Offset;
	FinalRot = Rot;

	if (Camera)
	{
		Camera->SetRelativeLocation(FinalOffset);
		Camera->SetRelativeRotation(FinalRot);
	}
}
