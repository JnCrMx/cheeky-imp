import bpy
import os
import json

base=os.path.splitext(bpy.path.basename(bpy.data.filepath))[0]

def export_obj(part):
	obj = bpy.data.objects[part]
	obj.hide_set(False)
	obj.select_set(True)
	
	bpy.ops.export_scene.obj(filepath=f"{base}_{part}.obj", use_selection=True, use_edges=False, use_normals=True, use_uvs=True, use_materials=False, use_blen_objects=False, use_triangles=True, keep_vertex_order=True)
	
	groups = {}
	for vertex in obj.data.vertices:
		for group in vertex.groups:
			g = "0"
			for group2 in obj.vertex_groups:
				if group2.index == group.group:
					g = group2.name
					break
			
			if not g in groups:
				groups[g] = {}
			groups[g][vertex.index] = group.weight
	
	with open(f"{base}_{part}.obj.json", 'w') as json_file:
		json.dump(groups, json_file, sort_keys=True)
	
	obj.select_set(False)

f = open("vhmesh.txt", "r")
for x in f:
  part=x.split()[0]
  export_obj(part)
f.close()

bpy.ops.wm.quit_blender()
