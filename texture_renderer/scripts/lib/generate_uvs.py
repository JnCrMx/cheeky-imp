import bpy

import sys
argv = sys.argv
argv = argv[argv.index("--") + 1:]

bpy.ops.object.delete()

bpy.ops.wm.obj_import(filepath=argv[0])
obj = bpy.context.selected_objects[0]

context = bpy.context
scene = context.scene
vl = context.view_layer

bpy.ops.object.select_all(action='DESELECT')

vl.objects.active = obj
obj.select_set(True)

bpy.ops.object.editmode_toggle()
bpy.ops.mesh.select_all(action='SELECT')
# bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.0002)
bpy.ops.uv.lightmap_pack(PREF_CONTEXT='ALL_FACES', PREF_BOX_DIV=64, PREF_MARGIN_DIV=0.1)
bpy.ops.object.editmode_toggle()

bpy.ops.wm.obj_export(filepath=argv[1],
    export_selected_objects=True, export_pbr_extensions=False, export_normals=False,
    export_uv=True, export_colors=False, export_materials=False, export_vertex_groups=False,
    export_object_groups=False, export_material_groups=False,
    export_triangulated_mesh=True
)
