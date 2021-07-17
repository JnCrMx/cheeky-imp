#!/usr/bin/env python 

import reg

vkreg = reg.Registry()
vkreg.loadFile("/usr/share/vulkan/registry/vk.xml")

print("#include \"reflection/vkreflection.hpp\"");
print("namespace CheekyLayer { namespace reflection {");
print("reflection_map struct_reflection_map = {");
for (name, description) in vkreg.typedict.items():
    if not 'category' in description.elem.attrib:
        continue
    if description.elem.attrib["category"] != "struct":
        continue
    if 'structextends' in description.elem.attrib:
        continue
    if name[-1].isupper():
        continue
    
    print(f"\t{{\n\t\t\"{name}\",\n\t\tstd::map<std::string, VkReflectInfo>{{")
    for member in description.getMembers():
        mType=member[0].text
        mName=member[1].text
        if mName == "sType" or mName == "pNext":
            continue

        mPointer = member[0].tail != None and '*' in member[0].tail
        if mPointer:
            mPointer = "true"
        else:
            mPointer = "false"
        
        print(f"\t\t\t{{\"{mName}\", VkReflectInfo{{ .name = \"{mName}\", .type = \"{mType}\", .pointer = {mPointer}, .offset = offsetof({name}, {mName}) }}}},")
        
    print("\t\t}\n\t},")
print("};");
print("}}");
