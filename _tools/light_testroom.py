import unreal

# Dress the TestRoom for an SCP:CB / Voices of the Void mood: dim, lonely interior,
# warm sparse pools of light over darkness, gentle fog, low exposure, vignette + grain.
LEVEL = "/Game/Maps/TestRoom"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.EditorActorSubsystem()
les.load_level(LEVEL)
acts = eas.get_all_level_actors()

# ---- 1) ceiling lights: dim, warm, pools with gaps (some dimmer for variation) ----
lights = [a for a in acts if isinstance(a, unreal.PointLight) and "CeilingLight" in a.get_actor_label()]
lights.sort(key=lambda a: a.get_actor_location().x)
for i, a in enumerate(lights):
    c = a.point_light_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_use_temperature(True)
    c.set_temperature(3100.0)                 # warm, sodium-ish interior
    dim = 1.0 if i % 2 == 0 else 0.45         # alternate bright/dim -> pools + gloom
    c.set_intensity(1800.0 * dim)
    c.set_attenuation_radius(560.0)
    c.set_source_radius(8.0)
    c.set_cast_shadows(True)
    c.set_editor_property("use_inverse_squared_falloff", True)
    c.set_editor_property("specular_scale", 0.4)
    c.set_editor_property("volumetric_scattering_intensity", 1.5)

# ---- 2) very low sky fill so shadows aren't pure black (lonely, not blind) ----
sky = next((a for a in acts if isinstance(a, unreal.SkyLight)), None)
if sky is None:
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(1000, 0, 160))
sky.set_actor_label("Sky_Fill")
slc = sky.get_component_by_class(unreal.SkyLightComponent)
slc.set_mobility(unreal.ComponentMobility.MOVABLE)
slc.set_intensity(0.06)
slc.set_editor_property("light_color", unreal.Color(120, 130, 150, 255))  # cool ambient

# ---- 3) gentle interior fog for depth / dusty air ----
fog = next((a for a in acts if isinstance(a, unreal.ExponentialHeightFog)), None)
if fog is None:
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(1000, 0, 0))
fog.set_actor_label("HeightFog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)

def tryset(obj, prop, val):
    try:
        obj.set_editor_property(prop, val)
    except Exception as e:
        unreal.log_warning("skip %s: %s" % (prop, e))

for prop, val in [
    ("fog_density", 0.055), ("FogDensity", 0.055),
    ("fog_height_falloff", 0.5), ("FogHeightFalloff", 0.5),
    ("volumetric_fog", True), ("bEnableVolumetricFog", True),
    ("volumetric_fog_extinction_scale", 1.5),
    ("volumetric_fog_distance", 3000.0),
]:
    tryset(fc, prop, val)

# ---- 4) post process volume: dark, graded, vignette + grain ----
ppv = next((a for a in acts if isinstance(a, unreal.PostProcessVolume)), None)
if ppv is None:
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(1000, 0, 160))
ppv.set_actor_label("Mood_PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")

def ov(flag, prop, val):
    try:
        s.set_editor_property(flag, True)
        s.set_editor_property(prop, val)
    except Exception as e:
        unreal.log_warning("skip pp %s: %s" % (prop, e))

# exposure: pull it down so the room reads dim and threatening
ov("override_auto_exposure_method", "auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
ov("override_auto_exposure_bias", "auto_exposure_bias", -2.0)
# bloom from the few light sources
ov("override_bloom_intensity", "bloom_intensity", 0.5)
ov("override_bloom_threshold", "bloom_threshold", 1.2)
# vignette closes the frame in (bodycam / dread)
ov("override_vignette_intensity", "vignette_intensity", 0.55)
# film grain - cheap-sensor texture (VotV look)
ov("override_film_grain_intensity", "film_grain_intensity", 0.35)
# desaturate + cool the shadows, lift contrast slightly -> liminal sterile feel
ov("override_color_saturation", "color_saturation", unreal.Vector4(0.82, 0.82, 0.9, 1.0))
ov("override_color_contrast", "color_contrast", unreal.Vector4(1.08, 1.08, 1.05, 1.0))
ov("override_color_gamma", "color_gamma", unreal.Vector4(0.97, 0.98, 1.02, 1.0))
# stronger ambient occlusion to dirty the corners
ov("override_ambient_occlusion_intensity", "ambient_occlusion_intensity", 0.75)
ov("override_ambient_occlusion_radius", "ambient_occlusion_radius", 80.0)
ppv.set_editor_property("settings", s)

les.save_current_level()
unreal.log("LIGHTING set: %d ceiling lights dimmed/warm, skyfill, fog, PP mood volume" % len(lights))
unreal.log("LIGHT TESTROOM DONE")
