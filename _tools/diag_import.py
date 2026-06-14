import unreal
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
skeleton = unreal.load_asset(SKEL)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

def imp(fbx, name):
    p = "/Game/Character/" + name
    if eal.does_asset_exist(p):
        eal.delete_asset(p)
    ask = unreal.FbxAnimSequenceImportData()
    ask.set_editor_property("convert_scene", True)
    ask.set_editor_property("convert_scene_unit", True)
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
    t.filename = ART + "/" + fbx
    t.destination_path = "/Game/Character"
    t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    t.options = ui
    tools.import_asset_tasks([t])
    cls = "MISSING"
    if eal.does_asset_exist(p):
        cls = unreal.load_asset(p).get_class().get_name()
    unreal.log("DIAG %s -> imported=%s class=%s" % (fbx, list(t.imported_object_paths), cls))

imp("Hunyuan_Walk.fbx", "DIAG_Walk")
imp("Hunyuan_Run.fbx", "DIAG_Run")
unreal.log("DIAG DONE")
