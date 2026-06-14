import unreal
eal = unreal.EditorAssetLibrary
def show(p):
    a = unreal.load_asset(p)
    if not a:
        unreal.log("MISSING %s" % p); return None
    unreal.log("=== %s : %s ===" % (p, a.get_class().get_name()))
    return a

abp = show("/Game/Character/ABP_Bodycam")
if abp:
    try:
        gen = abp.get_editor_property("generated_class")
        unreal.log("gen class: %s" % gen)
    except Exception as e:
        unreal.log("genclass err %s" % e)
    # list variables on the anim instance default object
    try:
        cdo = unreal.get_default_object(abp.generated_class())
        unreal.log("CDO: %s" % cdo)
    except Exception as e:
        unreal.log("cdo err %s" % e)

# find character / pawn blueprints in project
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for d in ar.get_assets_by_path("/Game", recursive=True):
    n = str(d.asset_name); cls = str(d.asset_class_path.asset_name)
    if any(k in n for k in ("Bodycam","Character","Pawn","Player","BP_")) or cls in ("Blueprint",):
        unreal.log("ASSET %s : %s @ %s" % (n, cls, d.package_name))
# input assets
for d in ar.get_assets_by_path("/Game", recursive=True):
    n = str(d.asset_name)
    if n.startswith("IA_") or n.startswith("IMC_") or "Input" in n:
        unreal.log("INPUT %s : %s" % (n, d.package_name))
unreal.log("INSPECT ABP DONE")
