"""Bereitet die 3D-Modelle (Tools/Import/Nature/3d/*.obj, Tripo, Graustufen-Vertexfarben) fuer Unreal vor:
Ursprung unten mittig, Front zeigt nach -X, Laenge 1 m; Export als FBX mit Vertexfarben nach Tools/Import/Nature/3d/fbx/.
Die Groesse im Spiel setzt der Code (Komponenten-Skalierung je Achse).

Aufruf: blender.exe -b --factory-startup --python Tools/blender_prepare_models.py
"""
import os
import bpy
from mathutils import Vector

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Import", "Nature", "3d")
OUT = os.path.join(D, "fbx")
os.makedirs(OUT, exist_ok=True)
MODELS = {
    "SM_TrainLoco": "tripo_convert_3572f6d0-cacb-4616-b19e-67fedca99eb7.obj",
    "SM_TrainCar": "tripo_convert_4d80d76c-5be6-4d22-9986-46720cc2bf55.obj",
    "SM_Minecart": "tripo_convert_f9ba7b3b-8c7a-4ba0-bb30-d77f83096a50.obj",
}

for name, fn in MODELS.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=os.path.join(D, fn))
    ob = bpy.context.selected_objects[0]
    ob.name = name
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bb = [Vector(c) for c in ob.bound_box]
    mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    # Ursprung: unten, mittig in X und Y
    shift = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
    for v in ob.data.vertices:
        v.co -= shift
    # weiche Schattierung fuer die runden Formen
    for p in ob.data.polygons:
        p.use_smooth = True
    if ob.data.color_attributes:
        ob.data.color_attributes.active_color = ob.data.color_attributes[0]
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, colors_type="SRGB",
                             apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE", add_leaf_bones=False)
    print("MODELL", name, "Groesse", tuple(round(x, 3) for x in (mx - mn)), "->", path)
