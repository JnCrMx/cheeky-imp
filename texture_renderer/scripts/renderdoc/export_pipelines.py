import json
import os

EXPORT_DIR = "#---CHANGE-ME---#"

def export(controller, draw):
	eventId = draw.eventId
	print(eventId)
	controller.SetFrameEvent(eventId, True)

	pipelineState = controller.GetVulkanPipelineState()
	pipeline = pipelineState.graphics
	descriptors = pipeline.descriptorSets

	baseDir = f"{EXPORT_DIR}/{eventId}"
	os.makedirs(baseDir, exist_ok = True)

	shader = pipelineState.fragmentShader
	shaderdata = shader.reflection.rawBytes
	with open(f"{baseDir}/shader.frag.spv", "wb") as f:
		f.write(shaderdata)

	setLayouts = []

	for setNum, set in enumerate(descriptors):
		bindings = []
		for i, b in enumerate(set.bindings):
			if not b.stageFlags & renderdoc.ShaderStageMask.Pixel:
				continue
			t = b.binds[0].type
			if t == renderdoc.BindType.ConstantBuffer or t == renderdoc.BindType.ReadOnlyBuffer:
				type = 6 # VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			if t == renderdoc.BindType.Sampler:
				type = 0 # VK_DESCRIPTOR_TYPE_SAMPLER
			if t == renderdoc.BindType.ImageSampler:
				type = 1 # VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			if t == renderdoc.BindType.ReadOnlyImage:
				type = 2 # VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
			binding = {"binding": i, "count": b.descriptorCount, "type": type}
			if type == 6 or type == 1 or type == 2: # VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER or VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
				data = []
				for j, bb in enumerate(b.binds):
					name = f"{setNum}_{i}_{j}"
					if type == 6: # VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
						name += ".buf"
						bufdata = controller.GetBufferData(bb.resourceResourceId, bb.byteOffset, bb.byteSize)
						with open(f"{baseDir}/{name}", "wb") as f:
							f.write(bufdata)
					else:
						name += ".png"
						texsave = renderdoc.TextureSave()
						texsave.resourceId = bb.resourceResourceId
						texsave.mip = 0
						texsave.slice.sliceIndex = 0
						texsave.alpha = renderdoc.AlphaMapping.Preserve
						texsave.destType = renderdoc.FileType.PNG
						controller.SaveTexture(texsave, f"{baseDir}/{name}")
					data.append(name)
				binding["data"] = data
			bindings.append(binding)
		setLayout = {"bindings": bindings}
		setLayouts.append(setLayout)

	with open(f"{baseDir}/pipeline.json", "w") as f:
		export = {"pipelineLayout": {"setLayouts": setLayouts}}
		json.dump(export, f, ensure_ascii=False, indent=4)

def exportCurrent(controller):
	draw = pyrenderdoc.CurAction()
	export(controller, draw)
def exportRenderPass(controller):
	action = pyrenderdoc.CurAction()
	while not action.flags & renderdoc.ActionFlags.EndPass:
		export(controller, action)
		action = action.next

pyrenderdoc.Replay().BlockInvoke(exportRenderPass)
