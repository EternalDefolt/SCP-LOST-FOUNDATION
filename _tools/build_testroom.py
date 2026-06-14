import unreal

# Build a long test room: floor, ceiling, 4 walls (engine cube meshes), ceiling lights,
# and a PlayerStart. Saved to /Game/Maps/TestRoom. Levers added in a second pass.
LEVEL = "/Game/Maps/TestRoom"
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")  # 100x100x100 uunits
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
ealib = unreal.EditorLevelLibrary

# fresh level
les.new_level(LEVEL)

# room interior dims (cm): long hall
L = 2000.0   # length (X)
W = 500.0    # width  (Y)
H = 320.0    # height (Z)
TH = 20.0    # wall thickness

def box(name, loc, scale):
    a = ealib.spawn_actor_from_object(CUBE, unreal.Vector(*loc))
    a.set_actor_label(name)
    a.set_actor_scale3d(unreal.Vector(scale[0] / 100.0, scale[1] / 100.0, scale[2] / 100.0))
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    if smc:
        smc.set_mobility(unreal.ComponentMobility.STATIC)
    return a

# floor at z=0 (top surface), ceiling at z=H
box("Floor",   (L / 2, 0, -TH / 2),       (L + TH, W + TH, TH))
box("Ceiling", (L / 2, 0, H + TH / 2),    (L + TH, W + TH, TH))
box("Wall_N",  (L / 2, W / 2 + TH / 2, H / 2), (L, TH, H))
box("Wall_S",  (L / 2, -W / 2 - TH / 2, H / 2), (L, TH, H))
box("Wall_W",  (-TH / 2, 0, H / 2),       (TH, W, H))
box("Wall_E",  (L + TH / 2, 0, H / 2),    (TH, W, H))

# ceiling lights down the hall (start ON)
lights = []
for i, x in enumerate((L * 0.2, L * 0.5, L * 0.8)):
    pl = ealib.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, 0, H - 30))
    pl.set_actor_label("CeilingLight_%d" % i)
    comp = pl.point_light_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_intensity(8000.0)
    comp.set_attenuation_radius(900.0)
    comp.set_light_color(unreal.LinearColor(1.0, 0.96, 0.88))
    lights.append(pl)

# player start near the west end, facing down the hall (+X)
ps = ealib.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(150, 0, 100))
ps.set_actor_label("PlayerStart")
ps.set_actor_rotation(unreal.Rotator(0, 0, 0), False)

les.save_current_level()
unreal.log("TESTROOM built: floor/ceiling/4 walls, %d lights, PlayerStart. Level=%s" % (len(lights), LEVEL))
unreal.log("BUILD TESTROOM DONE")
