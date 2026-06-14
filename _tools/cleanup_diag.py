import unreal
eal = unreal.EditorAssetLibrary
for p in ("/Game/Character/DIAG_Walk","/Game/Character/DIAG_Run",
          "/Game/Character/A_Run_PhysicsAsset","/Game/Character/A_Walk_PhysicsAsset",
          "/Game/Character/A_Zombie_PhysicsAsset"):
    if eal.does_asset_exist(p):
        eal.delete_asset(p); unreal.log("deleted %s" % p)
unreal.log("CLEANUP DONE")
