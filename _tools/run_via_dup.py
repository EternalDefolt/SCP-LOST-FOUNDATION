import unreal
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
path = "/Game/Character/A_Run"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = unreal.load_asset(SKEL)

# 1) nuke any stale A_Run (currently a SkeletalMesh) + physics asset
for p in (path, path + "_PhysicsAsset"):
    if eal.does_asset_exist(p):
        eal.delete_asset(p)

# 2) seed A_Run as an AnimSequence by duplicating the working A_Walk
eal.duplicate_asset("/Game/Character/A_Walk", path)
seed = unreal.load_asset(path)
unreal.log("SEED A_Run class=%s" % seed.get_class().get_name())

# 3) reimport the run FBX ON TOP of the existing AnimSequence (replace preserves type)
ask = unreal.FbxAnimSequenceImportData()
ask.set_editor_property("convert_scene", True)
ask.set_editor_property("convert_scene_unit", True)
ask.set_editor_property("import_uniform_scale", 1.0)
ask.set_editor_property("remove_redundant_keys", False)
ask.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ui = unreal.FbxImportUI()
ui.import_mesh = False
ui.import_as_skeletal = True
ui.import_animations = True
ui.import_materials = False
ui.import_textures = False
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
ui.set_editor_property("skeleton", skeleton)
ui.set_editor_property("anim_sequence_import_data", ask)
t = unreal.AssetImportTask()
t.filename = ART + "/Hunyuan_Run.fbx"
t.destination_path = "/Game/Character"
t.destination_name = "A_Run"
t.automated = True; t.replace_existing = True; t.save = True
t.options = ui
tools.import_asset_tasks([t])
after = unreal.load_asset(path)
unreal.log("AFTER REIMPORT A_Run class=%s imported=%s" % (after.get_class().get_name(), list(t.imported_object_paths)))
unreal.log("RUN VIA DUP DONE")
