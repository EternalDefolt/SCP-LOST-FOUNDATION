import unreal

# Import the IK walk (A_Walk) + the hand-painted Class D texture.
# Root fix v2: the rig is metres and the skeleton ref pose carries x100 on Hips,
# which the FBX anim wipes (ant-size bug). The OLD fix froze Hips position too,
# which would kill the new pelvis bob - so here we keep the imported position
# and rotation tracks and only force scale back to 100.
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"

skeleton = unreal.load_asset(SKEL)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
S = unreal.AnimSequenceService

# ---------- 1) animation ----------
ask = unreal.FbxAnimSequenceImportData()
ask.set_editor_property("convert_scene", True)
ask.set_editor_property("convert_scene_unit", True)
ask.set_editor_property("import_uniform_scale", 1.0)
ask.set_editor_property("remove_redundant_keys", False)
ask.set_editor_property("animation_length",
    unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
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
t.filename = ART + "/Hunyuan_Walk.fbx"
t.destination_path = "/Game/Character"
t.destination_name = "A_Walk"
t.automated = True
t.replace_existing = True
t.save = True
t.options = ui
tools.import_asset_tasks([t])
unreal.log("IMPORTED A_Walk -> %s" % list(t.imported_object_paths))

if eal.does_asset_exist("/Game/Character/A_Walk_PhysicsAsset"):
    eal.delete_asset("/Game/Character/A_Walk_PhysicsAsset")

# ---------- 2) root fix v2: keep pos/rot tracks, force scale 100 ----------
path = "/Game/Character/A_Walk"
anim = unreal.load_asset(path)
ctrl = anim.get_editor_property("controller")
dm = ctrl.get_model_interface()
n = dm.get_number_of_keys()
pos, rots = [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    pos.append(tr.translation)
    rots.append(tr.rotation)
sca = [unreal.Vector(100.0, 100.0, 100.0) for _ in range(n)]
ctrl.open_bracket("rootfix_v2", True)
ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
ctrl.close_bracket(True)
eal.save_asset(path, only_if_is_dirty=False)
poses = S.get_pose_at_time(path, 0.0, True)
zs = [p.transform.translation.z for p in poses]
unreal.log("ROOTFIX2 keys=%d height=%.1fcm hipsZ@0=%.2f" % (n, max(zs) - min(zs), pos[0].z))

# ---------- 3) painted texture + material ----------
ti = unreal.AssetImportTask()
ti.filename = ART + "/ClassD_Paint.png"
ti.destination_path = "/Game/Character"
ti.destination_name = "T_ClassD_Paint"
ti.automated = True
ti.replace_existing = True
ti.save = True
tools.import_asset_tasks([ti])
tex = unreal.load_asset("/Game/Character/T_ClassD_Paint")
unreal.log("TEXTURE: %s" % tex)

MAT = "/Game/Character/M_ClassD"
if not eal.does_asset_exist(MAT):
    mf = unreal.MaterialFactoryNew()
    mat = tools.create_asset("M_ClassD", "/Game/Character", unreal.Material, mf)
else:
    mat = unreal.load_asset(MAT)
MEL = unreal.MaterialEditingLibrary
# clean rebuild of the graph: one texture sample -> BaseColor
MEL.delete_all_material_expressions(mat)
ts = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -300, 0)
ts.texture = tex
MEL.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.recompile_material(mat)
eal.save_asset(MAT, only_if_is_dirty=False)

sk = unreal.load_asset("/Game/Character/SK_Hunyuan")
mats = sk.get_editor_property("materials")
new_mats = []
for m in mats:
    sm = unreal.SkeletalMaterial()
    sm.set_editor_property("material_interface", mat)
    sm.set_editor_property("material_slot_name", m.get_editor_property("material_slot_name"))
    new_mats.append(sm)
sk.set_editor_property("materials", new_mats)
eal.save_asset("/Game/Character/SK_Hunyuan", only_if_is_dirty=False)
unreal.log("MATERIAL assigned to %d slots" % len(new_mats))
unreal.log("WALK IK IMPORT DONE")
