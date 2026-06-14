import unreal
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
SK = "/Game/Character/SK_Hunyuan"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = unreal.load_asset(SKEL)

sk_data = unreal.FbxSkeletalMeshImportData()
sk_data.set_editor_property("update_skeleton_reference_pose", True)
sk_data.set_editor_property("use_t0_as_ref_pose", True)
sk_data.set_editor_property("convert_scene", True)
sk_data.set_editor_property("convert_scene_unit", True)
sk_data.set_editor_property("import_morph_targets", False)
sk_data.set_editor_property("preserve_smoothing_groups", True)

ui = unreal.FbxImportUI()
ui.import_mesh = True
ui.import_as_skeletal = True
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("skeleton", skeleton)
ui.set_editor_property("skeletal_mesh_import_data", sk_data)

t = unreal.AssetImportTask()
t.filename = ART + "/Hunyuan_Mesh.fbx"
t.destination_path = "/Game/Character"
t.destination_name = "SK_Hunyuan"
t.automated = True; t.replace_existing = True; t.save = True
t.options = ui
tools.import_asset_tasks([t])
unreal.log("SK reimported -> %s" % list(t.imported_object_paths))

# reassign painted material to all slots
mat = unreal.load_asset("/Game/Character/M_ClassD")
sk = unreal.load_asset(SK)
if mat and sk:
    mats = sk.get_editor_property("materials")
    new_mats = []
    for m in mats:
        sm = unreal.SkeletalMaterial()
        sm.set_editor_property("material_interface", mat)
        sm.set_editor_property("material_slot_name", m.get_editor_property("material_slot_name"))
        new_mats.append(sm)
    sk.set_editor_property("materials", new_mats)
    eal.save_asset(SK, False)
    unreal.log("material reassigned to %d slots" % len(new_mats))
unreal.log("UPDATE SKELETON DONE")
