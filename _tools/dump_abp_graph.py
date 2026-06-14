import unreal
abp = unreal.load_asset("/Game/Character/ABP_Bodycam")

# variables on the anim instance
try:
    bvars = unreal.BlueprintEditorLibrary.get_blueprint_variables(abp) if hasattr(unreal.BlueprintEditorLibrary,'get_blueprint_variables') else None
    unreal.log("vars helper: %s" % bvars)
except Exception as e:
    unreal.log("varerr %s" % e)

# enumerate graphs via subobjects
import unreal as u
def graphs_of(bp):
    out=[]
    try:
        for g in bp.get_editor_property("function_graphs"): out.append(("func",g.get_name()))
    except Exception as e: out.append(("func_err",str(e)))
    return out

# Anim graph nodes: walk the package for AnimGraphNode objects
loaded = unreal.load_object(None, "/Game/Character/ABP_Bodycam.ABP_Bodycam")
subs = []
unreal.log("ABP outer scan:")
# use AssetRegistry tags
ar = unreal.AssetRegistryHelpers.get_asset_registry()
d = ar.get_asset_by_object_path("/Game/Character/ABP_Bodycam.ABP_Bodycam")
try:
    tags = d.get_tags_and_values()
    for k,v in tags.items():
        unreal.log("TAG %s = %s" % (k, str(v)[:200]))
except Exception as e:
    unreal.log("tagerr %s" % e)

# default object property values for our anim vars
cdo = unreal.get_default_object(abp.generated_class())
for vn in ["Speed","AimPitch","AimYaw","WalkPlayRate"]:
    try:
        unreal.log("VAR %s = %s" % (vn, cdo.get_editor_property(vn)))
    except Exception as e:
        unreal.log("VAR %s MISSING (%s)" % (vn, e))
unreal.log("DUMP ABP DONE")
