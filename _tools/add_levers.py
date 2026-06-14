import unreal

LEVEL = "/Game/Maps/TestRoom"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ealib = unreal.EditorLevelLibrary
les.load_level(LEVEL)

# collect the ceiling lights
acts = unreal.EditorActorSubsystem().get_all_level_actors()
lights = [a for a in acts if isinstance(a, unreal.PointLight) and "CeilingLight" in a.get_actor_label()]
unreal.log("found %d ceiling lights" % len(lights))

# remove any previous levers (idempotent)
for a in acts:
    if isinstance(a, unreal.LightLever):
        unreal.EditorActorSubsystem().destroy_actor(a)

L = 2000.0
# two switches: one near each end of the hall, against the south wall, on the floor
spots = [unreal.Vector(L * 0.12, -200, 60), unreal.Vector(L * 0.88, -200, 60)]
made = 0
for i, p in enumerate(spots):
    lever = ealib.spawn_actor_from_class(unreal.LightLever, p)
    lever.set_actor_label("LightLever_%d" % i)
    lever.set_editor_property("Lights", lights)   # both levers toggle the whole room
    made += 1

les.save_current_level()
unreal.log("ADDED %d levers, each toggles %d lights" % (made, len(lights)))
unreal.log("ADD LEVERS DONE")
