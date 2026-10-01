"""Bereitet die Katze fuer UE vor: backt alle Animationen sauber mit 60 fps (Quaternionen, ohne Euler-Spruenge).

Aufruf:
  blender.exe -b --factory-startup --python Tools/blender_prepare_cat.py

Liest Tools/Import/Cat/Source/*.fbx (AccuRig-Exporte, gleiches Skelett) und schreibt Tools/Import/Cat/:
  SK_BlackCat.fbx (Modell), A_<Name>.fbx (je Animation, nur Skelett).
Zuordnung Quelle -> Name in CLIPS; fehlende Dateien werden uebersprungen.
Grund: Der direkte UE-Import der AccuRig-Dateien tastete mit 1920 fps ab und drehte die Huefte zeitweise um 180 Grad.
"""
import bpy
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "Import", "Cat", "Source")
DST = os.path.join(HERE, "Import", "Cat")
MODEL = "blackcat.fbx"
# Quelle -> (Zielname, Startframe, Endframe oder None = ganzer Clip)
CLIPS = {
    "run-2l-575663.fbx": ("A_CatRun", None, None),
    "run-1s-575662.fbx": ("A_CatRunStart", None, None),
    "run-3e-575664.fbx": ("A_CatRunStop", None, None),
    "flying idle.fbx": ("A_CatJump", None, None),
    "roll forward.fbx": ("A_CatRoll", 33, 112),
    "idle.fbx": ("A_CatIdle", None, None),
    "jump idle.fbx": ("A_CatJumpIdle", None, None),
    "crawling.fbx": ("A_CatCrawl", None, None),
}


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 60


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    return [o for o in bpy.data.objects if o not in before]


def export(path, objects, with_anim, frame_range=None):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    sc = bpy.context.scene
    if frame_range:
        sc.frame_start, sc.frame_end = frame_range
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False, bake_anim=with_anim, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
        bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL",
        mesh_smooth_type="FACE", armature_nodetype="NULL")


def make_in_place(arm, s, e, root="RL_BoneRoot", hip="CC_Base_Hip"):
    """Root-Knochen fix (Ruhelage), Huefte uebernimmt die komplette Koerperbewegung ohne horizontalen Weg.
    Hintergrund: Der Root dreht pro Schritt um +-37 Grad; mit UE-Root-Lock fehlte dieser Anteil und die Katze verdrehte sich."""
    sc = bpy.context.scene
    pr = arm.pose.bones[root]
    ph = arm.pose.bones[hip]
    mats = {}
    for f in range(s, e + 1):
        sc.frame_set(f)
        mh = ph.matrix.copy()
        rl = pr.matrix.to_translation()
        mh.translation.x -= rl.x
        mh.translation.y -= rl.y
        mats[f] = mh
    for pb in (pr, ph):
        pb.rotation_mode = "QUATERNION"
    prev = None
    for f in range(s, e + 1):
        sc.frame_set(f)
        pr.location = (0.0, 0.0, 0.0)
        pr.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pr.scale = (1.0, 1.0, 1.0)
        for path in ("location", "rotation_quaternion", "scale"):
            pr.keyframe_insert(path, frame=f)
        bpy.context.view_layer.update()
        ph.matrix = mats[f]
        q = ph.rotation_quaternion.copy()
        if prev is not None:
            q.make_compatible(prev)
            ph.rotation_quaternion = q
        prev = q
        ph.keyframe_insert("location", frame=f)
        ph.keyframe_insert("rotation_quaternion", frame=f)


clear()
objs = import_fbx(os.path.join(SRC, MODEL))
arm = [o for o in objs if o.type == "ARMATURE"][0]
arm.name = "Armature"
if arm.animation_data:
    arm.animation_data.action = None
export(os.path.join(DST, "SK_BlackCat.fbx"), objs, False)
print("CATPREP Modell exportiert")

for src, (name, f0, f1) in CLIPS.items():
    path = os.path.join(SRC, src)
    if not os.path.exists(path):
        print("CATPREP fehlt: %s" % src)
        continue
    clear()
    objs = import_fbx(path)
    arms = [o for o in objs if o.type == "ARMATURE"]
    if not arms or not arms[0].animation_data or not arms[0].animation_data.action:
        print("CATPREP keine Animation in %s" % src)
        continue
    a = arms[0]
    a.name = "Armature"
    act = a.animation_data.action
    s, e = int(act.frame_range[0]), int(act.frame_range[1])
    if f0 is not None:
        s, e = f0, f1 if f1 else e
    make_in_place(a, s, e)
    export(os.path.join(DST, name + ".fbx"), [a], True, (s, e))
    print("CATPREP %s -> %s (%d-%d)" % (src, name, s, e))
