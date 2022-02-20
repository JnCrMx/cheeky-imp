import bpy
import os

base=os.path.splitext(bpy.path.basename(bpy.data.filepath))[0]

def export_obj(part):
	obj = bpy.data.objects[part]
	obj.hide_set(False)
	obj.select_set(True)
	
	bpy.ops.export_scene.obj(filepath=f"{base}_{part}.obj", use_selection=True, use_edges=False, use_normals=False, use_uvs=True, use_materials=False, use_blen_objects=False, keep_vertex_order=True)
	
	obj.select_set(False)

f = open("vhmesh.txt", "r")
for x in f:
  part=x.split()[0]
  export_obj(part)
f.close()

bpy.ops.wm.quit_blender()
