import bpy
import mathutils
import json

def do_bone(part, bone, edit_bones, parent):
    if "head" in bone and "tail" in bone and not bone["head"][0] == None and not bone["tail"][0] == None:
        b = edit_bones.new(part+"_"+str(bone["group"]))
        head = mathutils.Vector(bone["head"]).xzy
        head.y *= -1
        tail = mathutils.Vector(bone["tail"]).xzy
        tail.y *= -1
        b.head = head
        b.tail = tail
        b.parent = parent
    
        for child in bone["children"]:
            do_bone(part, child, edit_bones, b)
    elif "children" in bone:
        for child in bone["children"]:
            do_bone(part, child, edit_bones, parent)

def load_bones(part):
    with open(bpy.path.abspath(f"//bones/{part}.json")) as f:
        bones = json.load(f)
    try:
        bpy.ops.object.mode_set(mode='OBJECT')
    except:
        pass
        
    mesh = bpy.data.objects[part]
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.object.parent_clear(type='CLEAR')
    
    name = f"Armature-{part}"
    if name in bpy.data.objects:
        obj = bpy.data.objects[name]
    else:
        bpy.ops.object.armature_add()
        obj = bpy.context.active_object
        obj.name = name
    bpy.context.view_layer.objects.active = obj
        
    obj.data.name = name
    
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    
    edit_bones = obj.data.edit_bones
    for b in edit_bones:
        edit_bones.remove(b)
    
    for tree in bones["trees"]:
        do_bone(part, tree, edit_bones, None)
    
    bpy.ops.object.mode_set(mode='OBJECT')
    mesh.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.parent_set(type='ARMATURE')
    bpy.ops.object.select_all(action='DESELECT')

def load_constraints(part):
    with open(bpy.path.abspath(f"//bones/{part}.json")) as f:
        bones = json.load(f)
    name = f"Armature-{part}"
    obj = bpy.data.objects[name]
    
    for bone in obj.pose.bones:
        for c in bone.constraints:
            bone.constraints.remove(c)
    
    for tree in bones["trees"]:
        if "parent" in tree:
            p = tree["parent"]
            pp = p["mesh"]
            obj2 = bpy.data.objects[f"Armature-{pp}"]
            
            bone = obj.pose.bones.get(part+"_"+str(tree["group"]))
            if bone is not None:
                crc = bone.constraints.new('CHILD_OF')
                crc.target = obj2
                crc.subtarget = pp+"_"+str(p["bone"])

with open(bpy.path.abspath("//vhmesh.txt"), "r") as f:
    for x in f:
        part=x.split()[0]
        load_bones(part)
with open(bpy.path.abspath("//vhmesh.txt"), "r") as f:
    for x in f:
        part=x.split()[0]
        load_constraints(part)
bpy.ops.object.mode_set(mode='OBJECT')
