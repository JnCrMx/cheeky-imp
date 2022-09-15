#!/usr/bin/env python

import reg

vkreg = reg.Registry()
vkreg.loadFile("/usr/share/vulkan/registry/vk.xml")

print("#include \"reflection/vkreflection.hpp\"")
print("#include \"reflection/custom_structs.hpp\"")
print("namespace CheekyLayer { namespace reflection {")
print("reflection_map struct_reflection_map = {")
for (name, description) in vkreg.typedict.items():
    if not 'category' in description.elem.attrib:
        continue
    if description.elem.attrib["category"] != "struct":
        continue
    if 'structextends' in description.elem.attrib:
        continue
    if name[-1].isupper() and not name[-2].isdigit():
        continue

    print(f"\t{{\n\t\t\"{name}\",\n\t\tVkTypeInfo{{")
    print(f"\t\t\t.name = \"{name}\",")
    print(f"\t\t\t.size = sizeof({name}),")
    print(f"\t\t\t.members = inner_reflection_map{{")
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
        mArray = 'len' in member.attrib;
        if mArray:
            mArray = "true"
            mArrayLen = member.attrib['len'].replace("\\", "\\\\")
            mArrayInfo = f", .arrayLength = \"{mArrayLen}\""
        else:
            mArray = "false"
            mArrayInfo = ""

        print(f"\t\t\t\t{{\"{mName}\", VkReflectInfo{{ .name = \"{mName}\", .type = \"{mType}\", .pointer = {mPointer}, .array = {mArray}{mArrayInfo}, .offset = offsetof({name}, {mName}) }}}},")

    print("\t\t\t}\n\t\t}\n\t},")
print("\tVK_CUSTOM_STRUCTS")
print("};");


print("enum_map enum_reflection_map = {");
for enum in vkreg.tree.findall("enums"):
    name=enum.attrib["name"]
    if name == "VkStructureType":
        continue
    if name == "API Constants":
        continue
    if name == "VkResult":
        continue
    if name[-1].isupper():
        continue

    print(f"\t{{\n\t\t\"{name}\",\n\t\tinner_enum_map{{")
    for val in enum.findall(".//enum"):
        if "protect" in val.attrib:
            continue
        vName = val.attrib["name"]
        if "RESERVED" in vName or "_RESERVE_" in vName:
            continue
        if vName.endswith("_EXT"):
            continue
        if vName.endswith("_QCOM"):
            continue
        print(f"\t\t\t{{\"{vName}\", VkEnumEntry{{ .name = \"{vName}\", .value = (uint32_t) {vName}}}}},")
    print("\t\t}\n\t},")
print("};");

print("}}");
