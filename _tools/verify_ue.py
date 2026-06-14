import unreal

eal = unreal.EditorAssetLibrary

# rename the auto-named anim to A_Walk for tidiness (C++ finds it by class anyway)
if eal.does_asset_exist("/Game/Character/Hunyuan_Anim") and not eal.does_asset_exist("/Game/Character/A_Walk"):
    eal.rename_asset("/Game/Character/Hunyuan_Anim", "/Game/Character/A_Walk")
    eal.save_directory("/Game/Character", only_if_is_dirty=False)

sk = eal.load_asset("/Game/Character/SK_Hunyuan")
anim = eal.load_asset("/Game/Character/A_Walk")

print("VERIFY: SK loaded:", sk is not None)
print("VERIFY: Anim loaded:", anim is not None)

sk_skel = sk.get_editor_property("skeleton") if sk else None
an_skel = anim.get_editor_property("skeleton") if anim else None
print("VERIFY: SK skeleton:", sk_skel.get_name() if sk_skel else None)
print("VERIFY: Anim skeleton:", an_skel.get_name() if an_skel else None)
print("VERIFY: SAME SKELETON:", sk_skel == an_skel)

if anim:
    print("VERIFY: anim frames:", anim.get_editor_property("number_of_sampled_frames"))
    print("VERIFY: anim length(s):", anim.get_play_length())

if sk_skel:
    bones = sk_skel.get_editor_property("bone_tree")
    print("VERIFY: skeleton bone count:", len(bones))

print("VERIFY DONE")
