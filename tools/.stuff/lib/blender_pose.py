import bpy
import mathutils
import json
    
def pose_object(name, part, pose, obj):
    if not part in pose:
        return
    
    sk = obj.shape_key_add(name=name, from_mix=False)
    sk.interpolation = 'KEY_LINEAR'
    verts = obj.data.vertices

    for i in range(len(verts)):
        vertex = verts[i]
        pos = mathutils.Vector()
        
        sum=0.0
        for group in vertex.groups:
            sum+=group.weight
        for group in vertex.groups:
            group.weight /= sum
        
        for group in vertex.groups:
            g = "0"
            for group2 in obj.vertex_groups:
                if group2.index == group.group:
                    g = group2.name[len(part)+1:]
                    break
            mat = mathutils.Matrix([pose[part][g][i::4] for i in range(4)])
            pos += (vertex.co @ mat) * group.weight
        sk.data[i].co = pos


try:
    collection = bpy.data.collections["Pose"]
    for o in collection.objects:
        bpy.data.objects.remove(o)
except:
    collection = bpy.data.collections.new("Pose")
    bpy.context.scene.collection.children.link(collection)
    
with open("vhmesh.txt") as f:
    for x in f:
        part=x.split()[0]
            
        oldObj = bpy.data.objects[part]
        obj = oldObj.copy()
        obj.data = oldObj.data.copy()
        collection.objects.link(obj)
        
        sk_basis = obj.shape_key_add(name='Basis')
        sk_basis.interpolation = 'KEY_LINEAR'
        obj.data.shape_keys.use_relative = True
        
        with open("vhpose.txt") as ps:
            for y in ps:
                poseName=y.split()[0]
                with open(f"poses/{poseName}.json") as f:
                    pose = json.load(f)
                    pose_object(poseName, part, pose, obj)
