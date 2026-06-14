import unreal

eal = unreal.EditorAssetLibrary
sk = eal.load_asset("/Game/Character/SK_Hunyuan")
print("INSPECT: SK loaded:", sk is not None)
if sk:
    try:
        b = sk.get_bounds()
        print("INSPECT: box_extent (half-size cm):", b.box_extent)
        print("INSPECT: sphere_radius (cm):", b.sphere_radius)
        print("INSPECT: origin:", b.origin)
    except Exception as e:
        print("INSPECT: get_bounds failed:", e)
    try:
        ib = sk.get_editor_property("imported_bounds")
        print("INSPECT: imported_bounds:", ib)
    except Exception as e:
        print("INSPECT: imported_bounds failed:", e)
    skel = sk.get_editor_property("skeleton")
    print("INSPECT: skeleton:", skel.get_name() if skel else None)
    mats = sk.get_editor_property("materials")
    print("INSPECT: material slots:", len(mats))
    for i, m in enumerate(mats):
        mi = m.get_editor_property("material_interface")
        print("   slot", i, "material:", mi.get_name() if mi else None, "slot_name:", m.get_editor_property("material_slot_name"))
print("INSPECT DONE")
