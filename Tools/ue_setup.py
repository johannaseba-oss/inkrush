"""Legt alle Editor-Assets fuer Inkrush (Projekt ShadowCat) an (Katze, Materialien, Map, Blueprints).

Aufruf (Editor geschlossen, C++ vorher gebaut):
  UnrealEditor-Cmd.exe ShadowCat.uproject -run=pythonscript -script=Tools/ue_setup.py -unattended -nosplash -nullrhi

Schritte per Umgebungsvariable CAT_SETUP (Komma-Liste, Standard: alle): cat, materials, map, blueprints

Katze austauschen: Dateien in Tools/Import/Cat ersetzen (gleiche Namen) und Schritt "cat,blueprints" erneut ausfuehren:
  SK_BlackCat.fbx      Modell mit Skelett (ohne Animation)
  A_CatRun.fbx         Lauf-Schleife (Pflicht)
  A_CatRunStart.fbx    Anlauf (optional)
  A_CatRunStop.fbx     Abbremsen (optional)
  T_BlackCat.png       Farbtextur (optional)
"""
import os
import math
import struct
import wave
import re
import unreal

PROJECT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
IMPORT = os.path.join(PROJECT, "Tools", "Import", "Cat")
STEPS = os.environ.get("CAT_SETUP", "cat,fx,materials,sprites,boss,audio,ui,map,blueprints").split(",")
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
log = unreal.log

RESULT = {}


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def run_task(task):
    tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        unreal.log_warning("Import ohne Ergebnis: %s" % task.filename)
        return None
    return paths[0]


def base_task(fbx, dest, name):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    return task


def import_skeletal(fbx, dest, name):
    task = base_task(fbx, dest, name)
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.create_physics_asset = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.automated_import_should_detect_type = False
    sm = ui.skeletal_mesh_import_data
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    sm.convert_scene = True
    sm.convert_scene_unit = True
    sm.import_uniform_scale = 1.0
    task.options = ui
    return run_task(task)


def import_anim(fbx, dest, name, skeleton):
    task = base_task(fbx, dest, name)
    ui = unreal.FbxImportUI()
    ui.import_mesh = False
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = True
    ui.skeleton = skeleton
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    ui.automated_import_should_detect_type = False
    ad = ui.anim_sequence_import_data
    ad.convert_scene = True
    ad.convert_scene_unit = True
    ad.import_uniform_scale = 1.0
    # Mit 60 fps abtasten (sonst las UE die AccuRig-Dateien mit 1920 fps und die Huefte drehte sich zwischen den Keys)
    ad.set_editor_property("use_default_sample_rate", False)
    ad.set_editor_property("custom_sample_rate", 60)
    task.options = ui
    return run_task(task)


def import_texture(png, dest, name):
    task = base_task(png, dest, name)
    path = run_task(task)
    if path:
        tex = unreal.load_asset(path)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        # 4k-Vorlage: fuer das iPhone reichen 1024 px (Katze ist klein im Bild)
        tex.set_editor_property("max_texture_size", 1024)
        eal.save_loaded_asset(tex)
    return path


def import_static(fbx, dest, name, vertex_colors=False):
    task = base_task(fbx, dest, name)
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.automated_import_should_detect_type = False
    sd = ui.static_mesh_import_data
    sd.combine_meshes = True
    sd.generate_lightmap_u_vs = False
    sd.auto_generate_collision = False
    sd.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    sd.convert_scene = True
    sd.convert_scene_unit = True
    sd.import_uniform_scale = 1.0
    if vertex_colors:
        sd.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    task.options = ui
    return run_task(task)


def set_blendable_location(m, names):
    for nm in names:
        try:
            m.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, nm))
            return nm
        except Exception:
            continue
    return None


# ----------------------------------------------------------------------------------------------
# Katze
# ----------------------------------------------------------------------------------------------
def root_direction(anim, root_bone):
    """Laufrichtung der (noch nicht gelockten) Root-Bewegung im Mesh-Raum."""
    length = anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else unreal.AnimationLibrary.get_sequence_length(anim)
    try:
        a = unreal.AnimationLibrary.get_bone_pose_for_time(anim, root_bone, 0.0, False)
        b = unreal.AnimationLibrary.get_bone_pose_for_time(anim, root_bone, max(0.0, length - 0.02), False)
        return b.translation - a.translation
    except Exception as exc:
        unreal.log_warning("Root-Richtung nicht lesbar: %s" % exc)
        return None


if "cat" in STEPS:
    # frisch anlegen: Modell/Skelett aus Tools/blender_prepare_cat.py passen nicht zum alten Skelett-Asset
    if eal.does_directory_exist("/Game/Cat"):
        eal.delete_directory("/Game/Cat")
    ensure_dir("/Game/Cat")
    mesh_path = import_skeletal(os.path.join(IMPORT, "SK_BlackCat.fbx"), "/Game/Cat", "SK_BlackCat")
    log("Katze: %s" % mesh_path)
    mesh = unreal.load_asset(mesh_path) if mesh_path else None
    skeleton = mesh.skeleton if mesh else None
    if skeleton:
        eal.save_loaded_asset(skeleton)
        b = mesh.get_bounds()
        log("Katze Bounds: Mitte %s Ausdehnung %s" % (b.origin, b.box_extent))
        root_bone = "RL_BoneRoot"
        yaw = None
        for name in ("A_CatRun", "A_CatRunStart", "A_CatRunStop", "A_CatIdle", "A_CatRoll", "A_CatJump", "A_CatJumpIdle", "A_CatCrawl"):
            fbx = os.path.join(IMPORT, name + ".fbx")
            if not os.path.exists(fbx):
                continue
            ap = import_anim(fbx, "/Game/Cat", name, skeleton)
            anim = unreal.load_asset(ap) if ap else None
            if not anim:
                continue
            length = unreal.AnimationLibrary.get_sequence_length(anim)
            if name == "A_CatRun":
                d = root_direction(anim, root_bone)
                if d is not None:
                    dist = math.sqrt(d.x * d.x + d.y * d.y)
                    log("Lauf-Schleife: %.2f s, Root-Weg %.1f cm -> %.1f cm/s, Richtung (%.1f, %.1f)" % (length, dist, dist / max(length, 0.01), d.x, d.y))
                    if dist > 10:
                        yaw = -math.degrees(math.atan2(d.y, d.x))
                        RESULT["anim_root_speed"] = dist / max(length, 0.01)
            # Vorwaertsbewegung im Clip entfernen (Katze bleibt am Ort, Strecke bewegt sich)
            anim.set_editor_property("force_root_lock", True)
            anim.set_editor_property("enable_root_motion", False)
            eal.save_loaded_asset(anim)
            log("Anim: %s (%.2f s)" % (ap, length))
        if yaw is not None:
            RESULT["mesh_yaw"] = yaw
            log("Modell-Drehung (MeshYaw): %.1f" % yaw)
    tex_png = os.path.join(IMPORT, "T_BlackCat.png")
    if os.path.exists(tex_png):
        log("Textur: %s" % import_texture(tex_png, "/Game/Cat", "T_BlackCat"))


if "fx" in STEPS:
    ensure_dir("/Game/Fx")
    png = os.path.join(PROJECT, "Tools", "Import", "Fx", "T_FogPuff.png")
    if os.path.exists(png):
        path = run_task(base_task(png, "/Game/Fx", "T_FogPuff"))
        if path:
            tex = unreal.load_asset(path)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
            tex.set_editor_property("max_texture_size", 512)
            eal.save_loaded_asset(tex)
            log("Nebeltextur: %s" % path)
    else:
        unreal.log_warning("T_FogPuff.png fehlt (Tools/blender_make_fog.py ausfuehren)")


# ----------------------------------------------------------------------------------------------
# Materialien (mobil: wenige Instruktionen, keine Texturen ausser der Katze)
# ----------------------------------------------------------------------------------------------
def new_material(name, blend=None, shading=None):
    path = "/Game/Materials"
    ensure_dir(path)
    full = path + "/" + name
    m = None
    if eal.does_asset_exist(full):
        m = eal.load_asset(full)
        if isinstance(m, unreal.Material):
            mel.delete_all_material_expressions(m)
        else:
            eal.delete_asset(full)
            m = None
    if m is None:
        m = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    for flag in ("used_with_skeletal_mesh", "used_with_instanced_static_meshes"):
        try:
            m.set_editor_property(flag, True)
        except Exception as exc:
            unreal.log_warning("%s: %s (%s)" % (name, flag, exc))
    if blend is not None:
        m.set_editor_property("blend_mode", blend)
    if shading is not None:
        m.set_editor_property("shading_model", shading)
    return m


def vec_param(m, name, default, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*default))
    return n


def scalar_param(m, name, default, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def constant(m, v, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", v)
    return n


def custom(m, code, inputs, x, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", out)
    arr = []
    for nm in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        arr.append(ci)
    n.set_editor_property("inputs", arr)
    return n


def link(src, dst, pin):
    mel.connect_material_expressions(src, "", dst, pin)


def out(src, prop, pin=""):
    mel.connect_material_property(src, pin, prop)


CURVE_K = 2.5e-6   # Erdkruemmung: Absenkung = K * Abstand^2 (bei 100 m: 2,5 m), ab 200 m konstant


def add_curvature(m):
    # leichte "Erdkruemmung": alles sinkt mit dem Abstand zur Kamera quadratisch ab (nur Darstellung, Spiel-Logik unberuehrt)
    if m.get_editor_property("material_domain") != unreal.MaterialDomain.MD_SURFACE:
        return
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1400, 700)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, -1400, 800)
    c = custom(m, "float r = min(length(WP.xy - Cam.xy), 20000.0); return float3(0.0, 0.0, -%g * r * r);" % CURVE_K,
               ["WP", "Cam"], -1100, 750)
    link(wp, c, "WP"); link(cam, c, "Cam")
    out(c, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def finish(m):
    add_curvature(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("Material: %s" % m.get_path_name())


if "materials" in STEPS:
    # --- M_Mono: beleuchtete Graustufe mit Grundhelligkeit (kein Skylight noetig) ---
    m = new_material("M_Mono")
    col = vec_param(m, "Color", (0.5, 0.5, 0.5, 1), -700, -100)
    emi = scalar_param(m, "Emissive", 0.0, -700, 50)
    amb = scalar_param(m, "Ambient", 0.3, -700, 150)
    rough = scalar_param(m, "Roughness", 0.85, -700, 250)
    c = custom(m, "return Color.rgb * (Ambient + Emissive);", ["Color", "Ambient", "Emissive"], -350, 50)
    link(col, c, "Color"); link(amb, c, "Ambient"); link(emi, c, "Emissive")
    out(col, unreal.MaterialProperty.MP_BASE_COLOR)
    out(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    out(constant(m, 0.25, -350, 300), unreal.MaterialProperty.MP_SPECULAR)
    out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_Glow: unbeleuchtet leuchtend ---
    m = new_material("M_Glow", shading=unreal.MaterialShadingModel.MSM_UNLIT)
    col = vec_param(m, "Color", (1, 1, 1, 1), -700, -100)
    inten = scalar_param(m, "Intensity", 2.0, -700, 50)
    c = custom(m, "return Color.rgb * Intensity;", ["Color", "Intensity"], -350, 0)
    link(col, c, "Color"); link(inten, c, "Intensity")
    out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_Floor: einfach matt grau (keine Kacheln, keine Spiegelung) ---
    m = new_material("M_Floor")
    col = vec_param(m, "Color", (0.2, 0.2, 0.2, 1), -700, -100)
    amb = scalar_param(m, "Ambient", 0.25, -700, 50)
    e = custom(m, "return Color.rgb * Ambient;", ["Color", "Ambient"], -350, 50)
    link(col, e, "Color"); link(amb, e, "Ambient")
    out(col, unreal.MaterialProperty.MP_BASE_COLOR)
    out(constant(m, 1.0, -350, 200), unreal.MaterialProperty.MP_ROUGHNESS)
    out(constant(m, 0.0, -350, 280), unreal.MaterialProperty.MP_SPECULAR)
    out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_Cat: tiefschwarz, wie aus einem Guss Farbe/Tinte. Keine Textur-Details, keine grauen Raender:
    #     nur echte Glanzlichter vom Mondlicht und ein schmaler, harter Fake-Reflex (nass/lackiert). ---
    m = new_material("M_Cat")
    tex_asset = unreal.load_asset("/Game/Cat/T_BlackCat") if eal.does_asset_exist("/Game/Cat/T_BlackCat") else None
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -350)
    ts.set_editor_property("parameter_name", "Tex")
    if tex_asset:
        ts.set_editor_property("texture", tex_asset)
    eyeglow = scalar_param(m, "EyeGlow", 0.45, -1000, 550)
    refl = mel.create_material_expression(m, unreal.MaterialExpressionReflectionVectorWS, -1000, 50)
    fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -1000, -50)
    fres.set_editor_property("exponent", 5.0)
    flash = scalar_param(m, "Flash", 0.0, -1000, 150)
    gloss = scalar_param(m, "Roughness", 0.16, -1000, 250)
    shine = scalar_param(m, "Shine", 1.0, -1000, 350)
    moon = vec_param(m, "MoonDir", (0.62, -0.17, 0.77, 0), -1000, 450)
    eye = custom(m, "float g = dot(Tex.rgb, float3(0.3, 0.59, 0.11)); return smoothstep(0.22, 0.5, g);", ["Tex"], -700, -350, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(ts, eye, "Tex")
    base = custom(m, "float c = Eye * 0.85; return lerp(float3(c, c, c), float3(0.95, 0.95, 0.95), Flash);", ["Eye", "Flash"], -450, -250)
    link(eye, base, "Eye"); link(flash, base, "Flash")
    emis_code = "float3 R = normalize(Refl);\nfloat md = saturate(dot(R, normalize(Moon.xyz)));\nfloat hl = pow(md, 900.0) * 3.0 + pow(md, 120.0) * 0.25;\nfloat sky = pow(saturate(R.z), 6.0) * 0.05 * Fres;\nreturn (hl + sky) * Shine + Eye * EyeGlow + Flash * 1.3;"
    emis = custom(m, emis_code, ["Refl", "Moon", "Fres", "Shine", "Flash", "Eye", "EyeGlow"], -300, 150)
    link(refl, emis, "Refl"); link(moon, emis, "Moon"); link(fres, emis, "Fres"); link(shine, emis, "Shine"); link(flash, emis, "Flash")
    link(eye, emis, "Eye"); link(eyeglow, emis, "EyeGlow")
    out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    out(gloss, unreal.MaterialProperty.MP_ROUGHNESS)
    out(constant(m, 0.6, -300, 400), unreal.MaterialProperty.MP_SPECULAR)
    out(emis, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_Ink: Fahrbahn-Kachel. Custom Data 0 = sichtbare Bedeckung 0..1 in Laufrichtung (UV.x), weiche, leicht
    #     wellige Tintenkante. Weisse Stellen leuchten leicht und "atmen" (Luecken aus der Ferne sichtbar),
    #     Tinte glaenzt nass im Mondlicht. ---
    m = new_material("M_Ink")
    cd = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -1100, -250)
    cd.set_editor_property("data_index", 0)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1100, -150)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1100, -50)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1100, 50)
    refl = mel.create_material_expression(m, unreal.MaterialExpressionReflectionVectorWS, -1100, 150)
    # Tinte spiegelt den sichtbaren Mond (RunCamera: 52000/-20000/16000) -> Glanz weit vorn statt als Fleck nahe der Kamera
    moon = vec_param(m, "MoonDir", (0.897, -0.345, 0.276, 0), -1100, 250)
    glow = scalar_param(m, "WhiteGlow", 0.3, -1100, 350)
    flip = scalar_param(m, "FlipU", 0.0, -1100, 450)
    inkc = custom(m, "float x = lerp(UV.x, 1.0 - UV.x, Flip);\nfloat wob = 0.05 * sin(UV.y * 17.0 + WP.x * 0.013 + WP.y * 0.011);\nfloat ink = saturate((Cov - x + wob) * 12.0 + 0.5);\nink = (Cov >= 0.999) ? 1.0 : ink * step(0.001, Cov);\nreturn ink;",
                  ["Cov", "UV", "WP", "Flip"], -750, -150, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(cd, inkc, "Cov"); link(uv, inkc, "UV"); link(wp, inkc, "WP"); link(flip, inkc, "Flip")
    base = custom(m, "float edge = 1.0 - smoothstep(0.0, 0.035, min(UV.y, 1.0 - UV.y));\nfloat w = lerp(0.82, 0.015, Ink);\nw = lerp(w, lerp(0.55, 0.14, Ink), edge);\nreturn float3(w, w, w);",
                  ["Ink", "UV"], -450, -200)
    link(inkc, base, "Ink"); link(uv, base, "UV")
    rough = custom(m, "return lerp(0.95, 0.6, Ink);", ["Ink"], -450, 0, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(inkc, rough, "Ink")
    emis_code = "float pulse = 0.75 + 0.25 * sin(Time * 3.0);\nfloat3 R = normalize(Refl);\nfloat md = saturate(dot(R, normalize(Moon.xyz)));\nfloat wet = (pow(md, 250.0) * 0.3 + pow(saturate(R.z), 10.0) * 0.03) * Ink;\nreturn Base * 0.28 + (1.0 - Ink) * Glow * pulse + wet;"
    emis = custom(m, emis_code, ["Ink", "Time", "Refl", "Moon", "Glow", "Base"], -200, 150)
    link(inkc, emis, "Ink"); link(tm, emis, "Time"); link(refl, emis, "Refl"); link(moon, emis, "Moon"); link(glow, emis, "Glow"); link(base, emis, "Base")
    out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    out(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    # kein direktes Glanzlicht (der Cel-Shader zerlegte es in einen Ring-Fleck nahe der Kamera);
    # das nasse Mondglitzern kommt aus dem Emissive-Anteil "wet"
    out(constant(m, 0.0, -200, 350), unreal.MaterialProperty.MP_SPECULAR)
    out(emis, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_InkSheet: Tintenflaeche der Bombe. Glaenzend schwarz; Rand und Glanz kommen aus Weltkoordinaten, dadurch
    #     gehen die einzelnen (ueberlappenden) Stuecke nahtlos ineinander ueber. UV.y = quer zur Flaeche. ---
    m = new_material("M_InkSheet", blend=unreal.BlendMode.BLEND_MASKED, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1100, -200)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1100, -100)
    cv = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -1100, 0)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1100, 100)
    moon = vec_param(m, "MoonDir", (0.897, -0.345, 0.276, 0), -1100, 200)
    # lokale Lage quer zur Flaeche (Plane: -50..50) und je Seite "offen" (Instanzdaten 0 = -Y, 1 = +Y):
    # offene Seiten gehen nahtlos in die Nachbarflaeche ueber, nur echte Aussenkanten fransen gewellt aus
    o0 = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -400)
    o0.set_editor_property("data_index", 0)
    o1 = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -480)
    o1.set_editor_property("data_index", 1)
    cy = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -560)
    cy.set_editor_property("data_index", 2)
    hw = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -640)
    hw.set_editor_property("data_index", 3)
    # Seitenlage aus Welt-Y relativ zur Mitte der Flaeche (Instanzdaten 2/3, gerade Strecke)
    mask = custom(m, "float side = (WP.y - CY) / max(HW, 1.0) * 50.0;\n"
                     "float open = side < 0.0 ? O0 : O1;\n"
                     "float e = saturate(1.0 - abs(side) / 50.0);\n"
                     "e = open > 0.5 ? 1.0 : e;\n"
                     "float n = 0.5 + 0.22 * sin(WP.x * 0.021 + WP.y * 0.017) + 0.16 * sin(WP.x * 0.057 - WP.y * 0.043) + 0.1 * sin(WP.x * 0.13 + WP.y * 0.09);\n"
                     "return e * 1.6 - n * 0.75;",
                  ["WP", "O0", "O1", "CY", "HW"], -700, -150, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(wp, mask, "WP"); link(o0, mask, "O0"); link(o1, mask, "O1"); link(cy, mask, "CY"); link(hw, mask, "HW")
    gloss = custom(m, "float2 p = WP.xy;\n"
                      "float3 N = normalize(float3(0.1 * sin(p.x * 0.031 + T * 0.9) + 0.06 * sin(p.y * 0.052 + p.x * 0.013),\n"
                      "                            0.1 * cos(p.y * 0.036 - T * 0.7) + 0.06 * sin(p.x * 0.047), 1.0));\n"
                      "float3 V = normalize(Cam);\n"
                      "float3 R = reflect(-V, N);\n"
                      "float m = saturate(dot(R, normalize(Moon.xyz)));\n"
                      "float spec = pow(m, 300.0) * 0.6;\n"
                      "float fres = 0.0;\n"
                      "float v = spec + fres;\n"
                      "return float3(v, v, v);",
                   ["WP", "Cam", "T", "Moon"], -700, 100)
    link(wp, gloss, "WP"); link(cv, gloss, "Cam"); link(tm, gloss, "T"); link(moon, gloss, "Moon")
    out(constant(m, 0.0, -300, -250), unreal.MaterialProperty.MP_BASE_COLOR)
    out(gloss, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(mask, unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(m)

    # --- M_Beacon: leuchtende Saeule ueber den letzten fehlenden Abschnitten ---
    m = new_material("M_Beacon", blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 0)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -900, 100)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 200)
    ec = custom(m, "return float3(1.6, 1.6, 1.6) * (0.8 + 0.2 * sin(Time * 6.0));", ["Time"], -450, -100)
    link(tm, ec, "Time")
    oc = custom(m, "float ndv = saturate(abs(dot(normalize(N), normalize(V)))); return saturate(pow(ndv, 2.0) * 0.5);", ["N", "V"], -450, 100, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(nrm, oc, "N"); link(cam, oc, "V")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(oc, unreal.MaterialProperty.MP_OPACITY)
    finish(m)

    # --- M_FogPuff: weiche Nebelwolke als Billboard (Rauschtextur-Atlas, 4 Varianten), weich am Boden und nahe der Kamera ---
    m = new_material("M_FogPuff", blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    fogtex = unreal.load_asset("/Game/Fx/T_FogPuff") if eal.does_asset_exist("/Game/Fx/T_FogPuff") else None
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1100, -100)
    var = scalar_param(m, "Variant", 0.0, -1100, 0)
    atlas_uv = custom(m, "float v = floor(Variant + 0.5); return UV * 0.5 + float2(fmod(v, 2.0), floor(v / 2.0)) * 0.5;", ["UV", "Variant"], -800, -50, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    link(uv, atlas_uv, "UV"); link(var, atlas_uv, "Variant")
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -550, -100)
    ts.set_editor_property("parameter_name", "Tex")
    ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    if fogtex:
        ts.set_editor_property("texture", fogtex)
    mel.connect_material_expressions(atlas_uv, "", ts, "UVs")
    col = vec_param(m, "Color", (0.82, 0.82, 0.84, 1), -550, 150)
    inten = scalar_param(m, "Intensity", 0.6, -550, 250)
    opa = scalar_param(m, "Opacity", 0.85, -550, 350)
    depth = mel.create_material_expression(m, unreal.MaterialExpressionPixelDepth, -550, 450)
    ec = custom(m, "return Color.rgb * Intensity;", ["Color", "Intensity"], -250, 100)
    link(col, ec, "Color"); link(inten, ec, "Intensity")
    oc = custom(m, "float nearFade = saturate((Depth - 350.0) / 1000.0); return saturate(Tex.r * Opacity * nearFade);", ["Tex", "Opacity", "Depth"], -250, 300, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(ts, oc, "Tex"); link(opa, oc, "Opacity"); link(depth, oc, "Depth")
    df = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -50, 300)
    df.set_editor_property("fade_distance_default", 90.0)
    mel.connect_material_expressions(oc, "", df, "Opacity")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(df, unreal.MaterialProperty.MP_OPACITY)
    finish(m)

    # --- M_Soft: weiche transparente Kugel (Nebel, Aura, Lichtsaeule) ---
    m = new_material("M_Soft", blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    col = vec_param(m, "Color", (1, 1, 1, 1), -900, -200)
    inten = scalar_param(m, "Intensity", 1.0, -900, -100)
    opa = scalar_param(m, "Opacity", 0.5, -900, 0)
    soft = scalar_param(m, "Softness", 1.5, -900, 100)
    edge = scalar_param(m, "EdgeMode", 0.0, -900, 200)
    nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 300)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -900, 400)
    ec = custom(m, "return Color.rgb * Intensity;", ["Color", "Intensity"], -450, -150)
    link(col, ec, "Color"); link(inten, ec, "Intensity")
    oc = custom(m, "float ndv = saturate(abs(dot(normalize(N), normalize(V)))); float a = lerp(pow(ndv, Softness), pow(1.0 - ndv, Softness), EdgeMode); return saturate(a * Opacity);",
                ["N", "V", "Softness", "EdgeMode", "Opacity"], -450, 150, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(nrm, oc, "N"); link(cam, oc, "V"); link(soft, oc, "Softness"); link(edge, oc, "EdgeMode"); link(opa, oc, "Opacity")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(oc, unreal.MaterialProperty.MP_OPACITY)
    finish(m)

    # --- M_Disc: runder Bodenfleck (Ring = Markierung, sonst weicher Schatten) ---
    m = new_material("M_Disc", blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    col = vec_param(m, "Color", (1, 1, 1, 1), -900, -200)
    inten = scalar_param(m, "Intensity", 1.0, -900, -100)
    opa = scalar_param(m, "Opacity", 0.6, -900, 0)
    ring = scalar_param(m, "Ring", 0.0, -900, 100)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 200)
    ec = custom(m, "return Color.rgb * Intensity;", ["Color", "Intensity"], -450, -150)
    link(col, ec, "Color"); link(inten, ec, "Intensity")
    oc = custom(m, "float r = length(UV - 0.5) * 2.0; float blob = saturate(1.0 - r); blob *= blob; float rg = saturate(1.0 - abs(r - 0.82) * 9.0) * step(r, 1.0); return saturate(lerp(blob, rg, Ring) * Opacity);",
                ["UV", "Ring", "Opacity"], -450, 150, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(uv, oc, "UV"); link(ring, oc, "Ring"); link(opa, oc, "Opacity")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(oc, unreal.MaterialProperty.MP_OPACITY)
    finish(m)


    # --- M_FogBank: weisse Nebelbank neben der Strecke, weich zum Boden/zur Deko (DepthFade) ---
    m = new_material("M_FogBank", blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    col = vec_param(m, "Color", (0.85, 0.85, 0.85, 1), -900, -200)
    inten = scalar_param(m, "Intensity", 0.8, -900, -100)
    opa = scalar_param(m, "Opacity", 0.3, -900, 0)
    soft = scalar_param(m, "Softness", 1.6, -900, 100)
    fade = scalar_param(m, "FadeDistance", 250.0, -900, 180)
    nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 300)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -900, 400)
    ec = custom(m, "return Color.rgb * Intensity;", ["Color", "Intensity"], -450, -150)
    link(col, ec, "Color"); link(inten, ec, "Intensity")
    oc = custom(m, "float ndv = saturate(abs(dot(normalize(N), normalize(V)))); return saturate(pow(ndv, Softness) * Opacity);",
                ["N", "V", "Softness", "Opacity"], -450, 150, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(nrm, oc, "N"); link(cam, oc, "V"); link(soft, oc, "Softness"); link(opa, oc, "Opacity")
    df = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -200, 150)
    mel.connect_material_expressions(oc, "", df, "Opacity")
    mel.connect_material_expressions(fade, "", df, "FadeDistance")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(df, unreal.MaterialProperty.MP_OPACITY)
    finish(m)


    # --- M_ModelMono: Modelle aus anderen Projekten (Baum, Fels, ...) in Graustufen, matt ---
    m = new_material("M_ModelMono")
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200)
    ts.set_editor_property("parameter_name", "Tex")
    tint = scalar_param(m, "Tint", 0.75, -900, 50)
    amb = scalar_param(m, "Ambient", 0.25, -900, 150)
    base = custom(m, "float g = dot(Tex.rgb, float3(0.3, 0.59, 0.11)) * Tint; return float3(g, g, g);", ["Tex", "Tint"], -500, -150)
    link(ts, base, "Tex"); link(tint, base, "Tint")
    e = custom(m, "return Base * Ambient;", ["Base", "Ambient"], -250, 100)
    link(base, e, "Base"); link(amb, e, "Ambient")
    out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    out(constant(m, 0.9, -250, 250), unreal.MaterialProperty.MP_ROUGHNESS)
    out(constant(m, 0.2, -250, 330), unreal.MaterialProperty.MP_SPECULAR)
    out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- M_VertexMono: Modelle mit Graustufen-Vertexfarben (Zug, Waggon, Lore) ---
    m = new_material("M_VertexMono")
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, -200)
    tint = scalar_param(m, "Tint", 1.0, -900, 50)
    amb = scalar_param(m, "Ambient", 0.3, -900, 150)
    base = custom(m, "float g = dot(VC.rgb, float3(0.3, 0.59, 0.11)) * Tint; return float3(g, g, g);", ["VC", "Tint"], -500, -150)
    link(vc, base, "VC"); link(tint, base, "Tint")
    e = custom(m, "return Base * Ambient;", ["Base", "Ambient"], -250, 100)
    link(base, e, "Base"); link(amb, e, "Ambient")
    out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    out(constant(m, 0.45, -250, 250), unreal.MaterialProperty.MP_ROUGHNESS)
    out(constant(m, 0.5, -250, 330), unreal.MaterialProperty.MP_SPECULAR)
    out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- PP_CelMono: monochromes Cel Shading als Post-Process (mobiltauglich: nur Szenenfarbe + Tiefe) ---
    #     Helligkeit in wenige Stufen mit schmalem weichem Uebergang (wahrnehmungsgleich in sqrt-Raum),
    #     Glanzlichter > 1 bleiben, Konturen aus relativen Tiefenspruengen.
    m = new_material("PP_CelMono")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    loc = set_blendable_location(m, ["BL_SCENE_COLOR_BEFORE_BLOOM", "BL_BEFORE_TONEMAPPING", "BL_SCENE_COLOR_BEFORE_DOF"])
    log("PP_CelMono Blendable Location: %s" % loc)
    st_col = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, -200)
    st_col.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    st_dep = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 0)
    st_dep.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)
    p_bands = scalar_param(m, "Bands", 4.0, -900, 200)
    p_soft = scalar_param(m, "Softness", 0.08, -900, 300)
    p_thick = scalar_param(m, "LineThickness", 1.0, -900, 400)
    p_edge = scalar_param(m, "DepthEdge", 0.012, -900, 500)
    p_line = scalar_param(m, "LineStrength", 0.9, -900, 600)
    ids = {k: int(getattr(unreal.SceneTextureId, k).value) for k in ("PPI_POST_PROCESS_INPUT0", "PPI_SCENE_DEPTH")}
    code = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, ID_COL);
float2 px = View.ViewSizeAndInvSize.zw * thick;
float3 sc = SceneTextureLookup(uv, ID_COL, false).rgb;
float sd = SceneTextureLookup(uv, ID_DEPTH, false).r;
float lum = dot(sc, float3(0.3, 0.59, 0.11));
float x = sqrt(saturate(lum)) * bands;
float stepped = (floor(x) + smoothstep(0.5 - soft, 0.5 + soft, frac(x))) / bands;
stepped *= stepped;
float outL = (lum > 1.0) ? lum : stepped;
bool isSky = sd > 500000.0;
float dl = SceneTextureLookup(uv + float2(-px.x, 0), ID_DEPTH, false).r;
float dr = SceneTextureLookup(uv + float2( px.x, 0), ID_DEPTH, false).r;
float du = SceneTextureLookup(uv + float2(0, -px.y), ID_DEPTH, false).r;
float dd = SceneTextureLookup(uv + float2(0,  px.y), ID_DEPTH, false).r;
float dEdge = max(max(abs(dl - sd), abs(dr - sd)), max(abs(du - sd), abs(dd - sd))) / max(sd, 1.0);
float edge = isSky ? 0.0 : smoothstep(edgeK, edgeK * 2.5, dEdge);
// weit entfernte Kanten ausblenden (sonst flimmert der Horizont)
edge *= 1.0 - smoothstep(6000.0, 12000.0, sd);
outL = lerp(outL, 0.0, edge * lineStr);
return float4(outL, outL, outL, 1);
"""
    code = code.replace("ID_COL", str(ids["PPI_POST_PROCESS_INPUT0"])).replace("ID_DEPTH", str(ids["PPI_SCENE_DEPTH"]))
    c = custom(m, code, ["col", "depth", "bands", "soft", "thick", "edgeK", "lineStr"], -400, 0, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    mel.connect_material_expressions(st_col, "Color", c, "col")
    mel.connect_material_expressions(st_dep, "Color", c, "depth")
    link(p_bands, c, "bands"); link(p_soft, c, "soft"); link(p_thick, c, "thick"); link(p_edge, c, "edgeK"); link(p_line, c, "lineStr")
    out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

    # --- PP_Mirror: Bild horizontal spiegeln (Fluch der Gegner-Wuerfel), Amount 0 = aus, 1 = gespiegelt ---
    m = new_material("PP_Mirror")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    set_blendable_location(m, ["BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"])
    st = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 0)
    st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    amt = scalar_param(m, "Amount", 0.0, -700, 150)
    mc = custom(m, "float2 vuv = GetViewportUV(Parameters);\n"
                   "vuv.x = lerp(vuv.x, 1.0 - vuv.x, saturate(Amount));\n"
                   "float2 buv = ViewportUVToBufferUV(vuv);\n"
                   "return SceneTextureLookup(buv, 14, false).rgb + Dummy.rgb * 0.0;",
                ["Amount", "Dummy"], -350, 50)
    link(amt, mc, "Amount"); link(st, mc, "Dummy")
    out(mc, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

# ----------------------------------------------------------------------------------------------
# Natur-Modelle (aus ChibiArena uebernommen): Tools/Import/Nature/SM_<Name>.fbx + T_<Name>.jpg
# ----------------------------------------------------------------------------------------------
if "nature" in STEPS:
    ensure_dir("/Game/Nature")
    ndir = os.path.join(PROJECT, "Tools", "Import", "Nature")
    parent = unreal.load_asset("/Game/Materials/M_ModelMono")
    for fn in sorted(os.listdir(ndir)):
        if not fn.lower().endswith(".fbx"):
            continue
        name = os.path.splitext(fn)[0]
        short = name[3:] if name.startswith("SM_") else name
        mp = import_static(os.path.join(ndir, fn), "/Game/Nature", name)
        mesh = unreal.load_asset(mp) if mp else None
        if not mesh:
            continue
        tex = None
        for ext in (".jpg", ".png"):
            tp = os.path.join(ndir, "T_" + short + ext)
            if os.path.exists(tp):
                tpath = import_texture(tp, "/Game/Nature", "T_" + short)
                tex = unreal.load_asset(tpath) if tpath else None
                break
        if parent:
            mi_path = "/Game/Nature/MI_" + short
            if eal.does_asset_exist(mi_path):
                eal.delete_asset(mi_path)
            mi = tools.create_asset("MI_" + short, "/Game/Nature", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property("parent", parent)
            if tex:
                mel.set_material_instance_texture_parameter_value(mi, "Tex", tex)
            eal.save_loaded_asset(mi)
            for i in range(len(mesh.static_materials)):
                mesh.set_material(i, mi)
        eal.save_loaded_asset(mesh)
        b = mesh.get_bounds()
        log("Natur: %s Mitte %.0f,%.0f,%.0f Ausdehnung %.0f,%.0f,%.0f" % (name, b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))


# ----------------------------------------------------------------------------------------------
# Gezeichnete Deko + Riesen-Videos
#   Tools/Import/Nature/Tree*.png, House*.png -> /Game/Nature/T_Sprite_Tree1, T_Sprite_House1, ... (Bildtafeln)
#   Tools/Import/Enemy mp4/*.mp4             -> Content/Movies/<Name>.mp4 (Media Framework, per Dateipfad)
#   Materialien M_Sprite (maskiert, unbeleuchtet) und M_Video (additiv: schwarzer Videohintergrund verschwindet)
#   Die alten 3D-Naturmodelle (SM_/MI_/T_ in /Game/Nature) werden entfernt.
# ----------------------------------------------------------------------------------------------
if "sprites" in STEPS:
    import shutil
    ensure_dir("/Game/Nature")
    for a in eal.list_assets("/Game/Nature", recursive=True, include_folder=False):
        nm = a.split("/")[-1].split(".")[0]
        if nm.startswith("SM_") or nm.startswith("MI_") or (nm.startswith("T_") and not nm.startswith("T_Sprite_")):
            eal.delete_asset(a.split(".")[0])
            log("entfernt (3D-Deko): %s" % nm)
    ndir = os.path.join(PROJECT, "Tools", "Import", "Nature")
    first_tex = None
    for fn in sorted(os.listdir(ndir)):
        if not fn.lower().endswith(".png"):
            continue
        # "Tree 1.png" -> T_Sprite_Tree1, "street lamp.png" -> T_Sprite_StreetLamp, "Zaun.png" -> T_Sprite_Zaun
        name = "T_Sprite_" + "".join(w[:1].upper() + w[1:] for w in re.split(r"[^A-Za-z0-9]+", os.path.splitext(fn)[0]) if w)
        path = run_task(base_task(os.path.join(ndir, fn), "/Game/Nature", name))
        if path:
            tex = unreal.load_asset(path)
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("max_texture_size", 512)
            eal.save_loaded_asset(tex)
            first_tex = first_tex or tex
        log("Deko-Bild %s <- %s: %s" % (name, fn, path))

    # Effekt-Bilder: Tools/Import/Items/*.png -> /Game/Fx/T_Item_<Name> ("Cat Paw print.png" -> T_Item_CatPawPrint)
    idir = os.path.join(PROJECT, "Tools", "Import", "Items")
    ensure_dir("/Game/Fx")
    if os.path.isdir(idir):
        for fn in sorted(os.listdir(idir)):
            if not fn.lower().endswith(".png"):
                continue
            name = "T_Item_" + "".join(w[:1].upper() + w[1:] for w in re.split(r"[^A-Za-z0-9]+", os.path.splitext(fn)[0]) if w)
            path = run_task(base_task(os.path.join(idir, fn), "/Game/Fx", name))
            if path:
                tex = unreal.load_asset(path)
                tex.set_editor_property("srgb", True)
                # Overlays (Spruehnebel) in voller Breite, Symbole klein
                tex.set_editor_property("max_texture_size", 1024 if "Overlay" in name else 256)
                eal.save_loaded_asset(tex)
            log("Effekt-Bild %s <- %s: %s" % (name, fn, path))

    vdir = os.path.join(PROJECT, "Tools", "Import", "Enemy mp4")
    mdir = os.path.join(PROJECT, "Content", "Movies")
    # alte (umbenannte) Videos entfernen
    if os.path.isdir(mdir):
        shutil.rmtree(mdir)
    os.makedirs(mdir, exist_ok=True)
    if os.path.isdir(vdir):
        for fn in sorted(os.listdir(vdir)):
            if fn.lower().endswith(".mp4"):
                dst = re.sub(r"\s+", "_", os.path.splitext(fn)[0].strip()) + ".mp4"
                shutil.copyfile(os.path.join(vdir, fn), os.path.join(mdir, dst))
                log("Video %s -> Content/Movies/%s" % (fn, dst))

    # --- M_Sprite: gezeichnete Bildtafel, Alpha als Maske, unbeleuchtet (Linien bleiben scharf) ---
    m = new_material("M_Sprite", blend=unreal.BlendMode.BLEND_MASKED, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1000, -100)
    fu = scalar_param(m, "FlipU", 0.0, -1000, 0)
    fv = scalar_param(m, "FlipV", 0.0, -1000, 100)
    fuv = custom(m, "return float2(lerp(UV.x, 1.0 - UV.x, FU), lerp(UV.y, 1.0 - UV.y, FV));", ["UV", "FU", "FV"], -750, -50, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    link(uv, fuv, "UV"); link(fu, fuv, "FU"); link(fv, fuv, "FV")
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, -100)
    ts.set_editor_property("parameter_name", "Tex")
    if first_tex:
        ts.set_editor_property("texture", first_tex)
    mel.connect_material_expressions(fuv, "", ts, "UVs")
    br = scalar_param(m, "Brightness", 0.85, -500, 150)
    ec = custom(m, "return Tex.rgb * Brightness;", ["Tex", "Brightness"], -200, -100)
    mel.connect_material_expressions(ts, "RGB", ec, "Tex"); link(br, ec, "Brightness")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(m)

    # --- M_Video: Riesen-Video als additive Tafel am Himmel (schwarz = durchsichtig), Fade fuer Ueberblendungen ---
    m = new_material("M_Video", blend=unreal.BlendMode.BLEND_ADDITIVE, shading=unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1000, -100)
    fu = scalar_param(m, "FlipU", 0.0, -1000, 0)
    fv = scalar_param(m, "FlipV", 0.0, -1000, 100)
    fuv = custom(m, "return float2(lerp(UV.x, 1.0 - UV.x, FU), lerp(UV.y, 1.0 - UV.y, FV));", ["UV", "FU", "FV"], -750, -50, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    link(uv, fuv, "UV"); link(fu, fuv, "FU"); link(fv, fuv, "FV")
    ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, -100)
    ts.set_editor_property("parameter_name", "Video")
    if first_tex:
        ts.set_editor_property("texture", first_tex)
    mel.connect_material_expressions(fuv, "", ts, "UVs")
    fade = scalar_param(m, "Fade", 1.0, -500, 150)
    gain = scalar_param(m, "Gain", 1.0, -500, 250)
    ec = custom(m, "float3 c = saturate((Tex.rgb - 0.06) / 0.94); return c * Fade * Gain;", ["Tex", "Fade", "Gain"], -200, -100)
    mel.connect_material_expressions(ts, "RGB", ec, "Tex"); link(fade, ec, "Fade"); link(gain, ec, "Gain")
    out(ec, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


# ----------------------------------------------------------------------------------------------
# Riese (Endlos-Modus): Tools/Import/Boss/SK_Giant.fbx (+ optional A_GiantIdle.fbx, A_GiantAttack.fbx, T_Giant.png)
# Ohne Dateien bleibt der Platzhalter aus Grundformen aktiv.
# ----------------------------------------------------------------------------------------------
if "boss" in STEPS:
    bdir = os.path.join(PROJECT, "Tools", "Import", "Boss")
    sk_fbx = os.path.join(bdir, "SK_Giant.fbx")
    if os.path.exists(sk_fbx):
        if eal.does_directory_exist("/Game/Boss"):
            eal.delete_directory("/Game/Boss")
        ensure_dir("/Game/Boss")
        mp = import_skeletal(sk_fbx, "/Game/Boss", "SK_Giant")
        mesh = unreal.load_asset(mp) if mp else None
        if mesh and mesh.skeleton:
            eal.save_loaded_asset(mesh.skeleton)
            for name in ("A_GiantIdle", "A_GiantAttack"):
                fbx = os.path.join(bdir, name + ".fbx")
                if os.path.exists(fbx):
                    ap = import_anim(fbx, "/Game/Boss", name, mesh.skeleton)
                    anim = unreal.load_asset(ap) if ap else None
                    if anim:
                        anim.set_editor_property("force_root_lock", True)
                        eal.save_loaded_asset(anim)
            b = mesh.get_bounds()
            log("Riese: %s Ausdehnung %.0f,%.0f,%.0f (MeshScale in BP_BossGiant auf ~3500 cm Hoehe stellen)" % (mp, b.box_extent.x, b.box_extent.y, b.box_extent.z))
    else:
        log("Riese: kein Modell in Tools/Import/Boss - Platzhalter bleibt")


# ----------------------------------------------------------------------------------------------
# Musik und Soundeffekte
#   Tools/Import/Music/*.wav        -> /Game/Audio/Music/MUS_01, MUS_02, ... (Reihenfolge = Dateiname, natuerlich sortiert)
#   Tools/Import/Soundeffects/*.wav -> /Game/Audio/SFX_<Name> (vorher nach 16 Bit / 48 kHz / mono gewandelt,
#                                      damit auch 24-Bit-/Extensible-WAVs sicher importieren)
# Das Spiel spielt MUS_01.. nacheinander und beginnt danach wieder von vorn.
# ----------------------------------------------------------------------------------------------
def read_wav_any(path):
    data = open(path, "rb").read()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", "kein WAV: " + path
    pos = 12; fmt = None; pcm = None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]; size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    tag, ch, rate, bits = fmt
    n = len(pcm) // (bits // 8)
    if tag == 3 and bits == 32:
        vals = struct.unpack("<%df" % n, pcm[:n * 4])
    elif bits == 16:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % n, pcm[:n * 2])]
    elif bits == 24:
        vals = [int.from_bytes(pcm[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, n * 3, 3)]
    elif bits == 32:
        vals = [v / 2147483648.0 for v in struct.unpack("<%di" % n, pcm[:n * 4])]
    else:
        raise RuntimeError("Bittiefe %d nicht unterstuetzt: %s" % (bits, path))
    frames = n // ch
    mono = [sum(vals[i * ch:(i + 1) * ch]) / ch for i in range(frames)]
    return mono, rate


def resample(mono, rate, target=48000):
    if rate == target or not mono:
        return mono
    out_n = int(len(mono) * target / rate)
    out = []
    for i in range(out_n):
        x = i * rate / target; j = int(x); f = x - j
        a = mono[min(j, len(mono) - 1)]; b = mono[min(j + 1, len(mono) - 1)]
        out.append(a + (b - a) * f)
    return out


def normalize(mono, target_rms=0.18):
    if not mono:
        return mono
    rms = (sum(v * v for v in mono) / len(mono)) ** 0.5
    peak = max(abs(v) for v in mono)
    g = min(target_rms / max(rms, 1e-6), 0.95 / max(peak, 1e-6))
    return [v * g for v in mono]


def write_wav16(path, mono, rate=48000):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in mono))


def natural_key(name):
    stem = os.path.splitext(name)[0].lower()
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", stem)]


def import_sound(wav, dest, name, looping):
    task = base_task(wav, dest, name)
    path = run_task(task)
    if path:
        sw = unreal.load_asset(path)
        sw.set_editor_property("looping", looping)
        eal.save_loaded_asset(sw)
    return path


if "audio" in STEPS:
    mdir = os.path.join(PROJECT, "Tools", "Import", "Music")
    if eal.does_directory_exist("/Game/Audio"):
        eal.delete_directory("/Game/Audio")
    ensure_dir("/Game/Audio")
    ensure_dir("/Game/Audio/Music")
    if os.path.isdir(mdir):
        tracks = sorted([f for f in os.listdir(mdir) if f.lower().endswith(".wav")], key=natural_key)
        for i, f in enumerate(tracks):
            name = "MUS_%02d" % (i + 1)
            log("Musik %s <- %s: %s" % (name, f, import_sound(os.path.join(mdir, f), "/Game/Audio/Music", name, False)))
    sdir = os.path.join(PROJECT, "Tools", "Import", "Soundeffects")
    tmp = os.path.join(PROJECT, "Saved", "AudioConv")
    os.makedirs(tmp, exist_ok=True)
    if os.path.isdir(sdir):
        for f in sorted(os.listdir(sdir)):
            if not f.lower().endswith(".wav"):
                continue
            key = re.sub(r"[^A-Za-z0-9]+", "_", os.path.splitext(f)[0]).strip("_")
            mono, rate = read_wav_any(os.path.join(sdir, f))
            mono = normalize(resample(mono, rate))
            out = os.path.join(tmp, "SFX_" + key + ".wav")
            write_wav16(out, mono)
            looping = "step" in key.lower()
            log("Sound SFX_%s (%.1f s, Schleife %s): %s" % (key, len(mono) / 48000.0, looping, import_sound(out, "/Game/Audio", "SFX_" + key, looping)))


# ----------------------------------------------------------------------------------------------
# Item-Symbole: Tools/Import/UI/*.png -> /Game/UI/T_Icon_<Name> ("Ink Bomb item.png" -> T_Icon_InkBomb)
# ----------------------------------------------------------------------------------------------
if "ui" in STEPS:
    udir = os.path.join(PROJECT, "Tools", "Import", "UI")
    ensure_dir("/Game/UI")
    if os.path.isdir(udir):
        for f in sorted(os.listdir(udir)):
            if not f.lower().endswith(".png"):
                continue
            stem = re.sub(r"(?i)\bitem\b", " ", os.path.splitext(f)[0])
            name = "T_Icon_" + "".join(w[:1].upper() + w[1:] for w in re.split(r"[^A-Za-z0-9]+", stem) if w)
            path = run_task(base_task(os.path.join(udir, f), "/Game/UI", name))
            if path:
                tex = unreal.load_asset(path)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                tex.set_editor_property("srgb", True)
                # Overlays (Spruehnebel) in voller Breite, Symbole klein
                tex.set_editor_property("max_texture_size", 1024 if "Overlay" in name else 256)
                eal.save_loaded_asset(tex)
            log("UI-Symbol %s <- %s: %s" % (name, f, path))


# ----------------------------------------------------------------------------------------------
# 3D-Modelle: Tools/Import/Nature/3d/fbx/SM_*.fbx (aus Tools/blender_prepare_models.py) -> /Game/Models/SM_*
#   Vertexfarben werden uebernommen, Material M_VertexMono
# ----------------------------------------------------------------------------------------------
if "models" in STEPS:
    ensure_dir("/Game/Models")
    mdir = os.path.join(PROJECT, "Tools", "Import", "Nature", "3d", "fbx")
    vmat = unreal.load_asset("/Game/Materials/M_VertexMono")
    if os.path.isdir(mdir):
        for fn in sorted(os.listdir(mdir)):
            if not fn.lower().endswith(".fbx"):
                continue
            name = os.path.splitext(fn)[0]
            mp = import_static(os.path.join(mdir, fn), "/Game/Models", name, vertex_colors=True)
            mesh = unreal.load_asset(mp) if mp else None
            if mesh and vmat:
                for i in range(len(mesh.static_materials)):
                    mesh.set_material(i, vmat)
                eal.save_loaded_asset(mesh)
            b = mesh.get_bounding_box() if mesh else None
            log("Modell %s: %s (Groesse %s)" % (name, mp, (b.max - b.min) if b else None))


# ----------------------------------------------------------------------------------------------
# Map (leer; die Welt entsteht zur Laufzeit)
# ----------------------------------------------------------------------------------------------
if "map" in STEPS:
    ensure_dir("/Game/Maps")
    if eal.does_asset_exist("/Game/Maps/Run"):
        log("Map existiert bereits")
    else:
        ok = False
        try:
            les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            ok = les.new_level("/Game/Maps/Run")
            les.save_current_level()
        except Exception as exc:
            unreal.log_warning("LevelEditorSubsystem: %s" % exc)
        if not ok:
            ok = unreal.EditorLevelLibrary.new_level("/Game/Maps/Run")
            unreal.EditorLevelLibrary.save_current_level()
        log("Map angelegt: %s" % ok)


# ----------------------------------------------------------------------------------------------
# Blueprints (Einstellungen im Editor aenderbar, Katze austauschbar)
# ----------------------------------------------------------------------------------------------
def make_bp(name, parent):
    path = "/Game/Blueprints/" + name
    ensure_dir("/Game/Blueprints")
    bp = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = tools.create_asset(name, "/Game/Blueprints", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp


if "blueprints" in STEPS:
    bp = make_bp("BP_RunnerCat", unreal.RunnerCat)
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/BP_RunnerCat"))
    if "mesh_yaw" in RESULT:
        cdo.set_editor_property("mesh_yaw", RESULT["mesh_yaw"])
    if "anim_root_speed" in RESULT:
        cdo.set_editor_property("anim_root_speed", RESULT["anim_root_speed"])
    log("BP_RunnerCat: MeshYaw %.1f, AnimRootSpeed %.1f, Mesh %s" % (cdo.get_editor_property("mesh_yaw"), cdo.get_editor_property("anim_root_speed"), cdo.get_editor_property("cat_mesh")))
    eal.save_loaded_asset(bp, False)

    bp = make_bp("BP_TrackDirector", unreal.TrackDirector)
    eal.save_loaded_asset(bp, False)
    log("BP_TrackDirector angelegt")

log("ShadowCat Setup fertig: %s" % ",".join(STEPS))
