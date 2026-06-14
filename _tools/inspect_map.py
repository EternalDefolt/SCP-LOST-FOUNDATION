import unreal

les = unreal.LevelEditorSubsystem
unreal.EditorLoadingAndSavingUtils.load_map("/Game/BODYCAMTESTMAP")

actors = unreal.EditorLevelLibrary.get_all_level_actors()
print("MAP: total actors:", len(actors))
for a in actors:
    cls = a.get_class().get_name()
    loc = a.get_actor_location()
    name = a.get_actor_label()
    line = "MAP ACTOR: cls=%s label=%s loc=(%.1f,%.1f,%.1f)" % (cls, name, loc.x, loc.y, loc.z)
    # auto possess for pawns
    try:
        app = a.get_editor_property("auto_possess_player")
        line += " auto_possess=%s" % str(app)
    except Exception:
        pass
    print(line)
    # floor bounds
    if "Floor" in name or "Floor" in cls or "StaticMeshActor" in cls:
        try:
            origin, extent = a.get_actor_bounds(False)
            print("   BOUNDS origin=(%.1f,%.1f,%.1f) extent=(%.1f,%.1f,%.1f)" % (
                origin.x, origin.y, origin.z, extent.x, extent.y, extent.z))
        except Exception as e:
            print("   bounds failed:", e)

print("MAP INSPECT DONE")
