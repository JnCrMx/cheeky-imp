import bpy

import sys
argv = sys.argv
argv = argv[argv.index("--") + 1:]

bpy.ops.object.delete()

bpy.ops.import_scene.obj(filepath=argv[0])
obj = bpy.context.selected_objects[0]

context = bpy.context
scene = context.scene
vl = context.view_layer

bpy.ops.object.select_all(action='DESELECT')

vl.objects.active = obj
obj.select_set(True)

bpy.ops.object.editmode_toggle()
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.0002)
bpy.ops.object.editmode_toggle()

bpy.ops.export_scene.obj(filepath=argv[1], 
    use_selection=True, use_edges=False, use_normals=False, 
    use_uvs=True, use_materials=False, use_blen_objects=False, 
    use_triangles=True, keep_vertex_order=True
)
