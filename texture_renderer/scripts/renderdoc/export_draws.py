import sys
import os

EXPORT_DIR = "#---CHANGE-ME---#"

# Alias renderdoc for legibility
rd = renderdoc

# We'll need the struct data to read out of bytes objects
import struct

# We base our data on a MeshFormat, but we add some properties
class MeshData(rd.MeshFormat):
    indexOffset = 0
    name = ''

# Unpack a tuple of the given format, from the data
def unpackData(fmt, data):
    # We don't handle 'special' formats - typically bit-packed such as 10:10:10:2
    if fmt.Special():
        raise RuntimeError("Packed formats are not supported!")

    formatChars = {}
    #                                 012345678
    formatChars[rd.CompType.UInt]  = "xBHxIxxxL"
    formatChars[rd.CompType.SInt]  = "xbhxixxxl"
    formatChars[rd.CompType.Float] = "xxexfxxxd" # only 2, 4 and 8 are valid

    # These types have identical decodes, but we might post-process them
    formatChars[rd.CompType.UNorm] = formatChars[rd.CompType.UInt]
    formatChars[rd.CompType.UScaled] = formatChars[rd.CompType.UInt]
    formatChars[rd.CompType.SNorm] = formatChars[rd.CompType.SInt]
    formatChars[rd.CompType.SScaled] = formatChars[rd.CompType.SInt]

    # We need to fetch compCount components
    vertexFormat = str(fmt.compCount) + formatChars[fmt.compType][fmt.compByteWidth]

    # Unpack the data
    value = struct.unpack_from(vertexFormat, data, 0)

    # If the format needs post-processing such as normalisation, do that now
    if fmt.compType == rd.CompType.UNorm:
        divisor = float((2 ** (fmt.compByteWidth * 8)) - 1)
        value = tuple(float(i) / divisor for i in value)
    elif fmt.compType == rd.CompType.SNorm:
        maxNeg = -float(2 ** (fmt.compByteWidth * 8)) / 2
        divisor = float(-(maxNeg-1))
        value = tuple((float(i) if (i == maxNeg) else (float(i) / divisor)) for i in value)

    # If the format is BGRA, swap the two components
    if fmt.BGRAOrder():
        value = tuple(value[i] for i in [2, 1, 0, 3])

    return value


# Get a list of MeshData objects describing the vertex outputs at this draw
def getMeshOutputs(controller, postvs):
    meshOutputs = []
    posidx = 0

    vs = controller.GetPipelineState().GetShaderReflection(rd.ShaderStage.Vertex)

    # Repeat the process, but this time sourcing the data from postvs.
    # Since these are outputs, we iterate over the list of outputs from the
    # vertex shader's reflection data
    for attr in vs.outputSignature:
        # Copy most properties from the postvs struct
        meshOutput = MeshData()
        meshOutput.indexResourceId = postvs.indexResourceId
        meshOutput.indexByteOffset = postvs.indexByteOffset
        meshOutput.indexByteStride = postvs.indexByteStride
        meshOutput.baseVertex = postvs.baseVertex
        meshOutput.indexOffset = 0
        meshOutput.numIndices = postvs.numIndices

        # The total offset is the attribute offset from the base of the vertex,
        # as calculated by the stride per index
        meshOutput.vertexByteOffset = postvs.vertexByteOffset
        meshOutput.vertexResourceId = postvs.vertexResourceId
        meshOutput.vertexByteStride = postvs.vertexByteStride

        # Construct a resource format for this element
        meshOutput.format = rd.ResourceFormat()
        meshOutput.format.compByteWidth = rd.VarTypeByteSize(attr.varType)
        meshOutput.format.compCount = attr.compCount
        meshOutput.format.compType = rd.VarTypeCompType(attr.varType)
        meshOutput.format.type = rd.ResourceFormatType.Regular

        meshOutput.name = attr.semanticIdxName if attr.varName == '' else attr.varName

        if attr.systemValue == rd.ShaderBuiltin.Position:
            posidx = len(meshOutputs)

        meshOutputs.append(meshOutput)

    # Shuffle the position element to the front
    if posidx > 0:
        pos = meshOutputs[posidx]
        del meshOutputs[posidx]
        meshOutputs.insert(0, pos)

    accumOffset = 0

    for i in range(0, len(meshOutputs)):
        meshOutputs[i].vertexByteOffset += accumOffset

        # Note that some APIs such as Vulkan will pad the size of the attribute here
        # while others will tightly pack
        fmt = meshOutputs[i].format

        accumOffset += (8 if fmt.compByteWidth > 4 else 4) * fmt.compCount

    return meshOutputs

def getIndices(controller, mesh):
    # Get the character for the width of index
    indexFormat = 'B'
    if mesh.indexByteStride == 2:
        indexFormat = 'H'
    elif mesh.indexByteStride == 4:
        indexFormat = 'I'

    # Duplicate the format by the number of indices
    indexFormat = str(mesh.numIndices) + indexFormat

    # If we have an index buffer
    if mesh.indexResourceId != rd.ResourceId.Null():
        # Fetch the data
        ibdata = controller.GetBufferData(mesh.indexResourceId, mesh.indexByteOffset, 0)

        # Unpack all the indices, starting from the first index to fetch
        offset = mesh.indexOffset * mesh.indexByteStride
        indices = struct.unpack_from(indexFormat, ibdata, offset)

        # Apply the baseVertex offset
        return [i + mesh.baseVertex for i in indices]
    else:
        # With no index buffer, just generate a range
        return tuple(range(mesh.numIndices))

COORDS = ["x", "y", "z", "w"]
def export(draw, instance, controller):
    postvs = controller.GetPostVSData(instance, 0, rd.MeshDataStage.VSOut)
    meshOutputs = getMeshOutputs(controller, postvs)

    outfile = f"{EXPORT_DIR}/{draw.eventId}-{instance}.csv"
    if os.path.isfile(outfile):
        print(outfile+" SKIPPED")
        return
    print(outfile)
    with open(outfile, "w") as f:
        indices = getIndices(controller, meshOutputs[0])
        #print("Mesh configuration:")
        f.write("VTX, IDX")
        buffers = []
        for attr in meshOutputs:
            for i in range(0, attr.format.compCount):
                f.write(f", {attr.name}.{COORDS[i]}")
            #print("\t%s:" % attr.name)
            #print("\t\t- vertex: %s / %d stride" % (attr.vertexResourceId, attr.vertexByteStride))
            #print("\t\t- format: %s x %s @ %d" % (attr.format.compType, attr.format.compCount, attr.vertexByteOffset))
            buffers.append(controller.GetBufferData(attr.vertexResourceId, attr.vertexByteOffset, 0))
        f.write("\n")

        for i in range(0, len(indices)):
            idx = indices[i]

            #print("Vertex %d is index %d:" % (i, idx))

            f.write(f"{i}, {idx}")

            for i, attr in enumerate(meshOutputs):
                f.write(", ")
                offset = attr.vertexByteStride * idx
                data = buffers[i][offset:offset+64]

                # Get the value from the data
                value = unpackData(attr.format, data)

                # We don't go into the details of semantic matching here, just print both
                f.write(", ".join([str(f) for f in value]))
            f.write("\n")

def process(draw, controller):
    controller.SetFrameEvent(draw.eventId, True)
    for i in range(0, draw.numInstances):
        export(draw, i, controller)

def exportDraws(controller):
    os.makedirs(EXPORT_DIR, exist_ok=True)
    start = pyrenderdoc.CurAction()

    action = start
    while not action.flags & renderdoc.ActionFlags.EndPass:
        process(action, controller)
        action = action.next

pyrenderdoc.Replay().BlockInvoke(exportDraws)
