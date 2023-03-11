import bpy
from os import path
import os
import json

def import_obj(part):
	bpy.ops.import_scene.obj(filepath=f"base_{part}.obj")
	obj = bpy.context.selected_objects[0]

	for subpart in obj.material_slots.keys():
		if(path.exists(f"texture/{subpart}/base.png")):
			mat = obj.material_slots[subpart].material
			bsdf = mat.node_tree.nodes["Principled BSDF"]
			color_input = bsdf.inputs['Base Color']

			if(os.getenv('BLENDER_SHADER_TYPE') == 'toon'):
				mat.node_tree.nodes.remove(bsdf)
				bsdf = mat.node_tree.nodes.new("ShaderNodeBsdfToon")
				mat.node_tree.links.new(mat.node_tree.nodes["Material Output"].inputs["Surface"], bsdf.outputs["BSDF"])
				color_input = bsdf.inputs['Color']

			texImage = mat.node_tree.nodes.new('ShaderNodeTexImage')
			texImage.image = bpy.data.images.load(f"texture/{subpart}/base.png")
			mat.node_tree.links.new(color_input, texImage.outputs['Color'])

	if(path.exists(f"base_{part}.obj.json")):
		with open(f"base_{part}.obj.json") as f:
			data = json.load(f)
			for group, vertices in data.items():
				g = obj.vertex_groups.new(name = part+"_"+group)
				for index, weight in vertices.items():
					g.add([int(index)], weight, 'REPLACE')

	obj.name = part
	obj.data.use_auto_smooth = True
	obj.select_set(False)

bpy.context.scene.render.engine = 'CYCLES'
bpy.ops.object.delete()

f = open("vhmesh.txt", "r")
for x in f:
	part=x.split()[0]
	import_obj(part)
f.close()

bpy.data.texts.load(os.getenv("VH_LIB")+"/blender_pose.py")
bpy.data.texts.load(os.getenv("VH_LIB")+"/blender_bone_import.py")

bpy.ops.file.pack_all()

bpy.ops.wm.save_as_mainfile(filepath=os.getcwd()+"/base.blend")
bpy.ops.wm.quit_blender()
