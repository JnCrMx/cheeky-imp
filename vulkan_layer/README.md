# Cheeky Layer / ``vulkan_layer``

A configurable and programmable Vulkan layer made for hooking into games, and extracting and replacing textures, meshes, shaders and much more!

## Features
- Extract textures, meshes (raw buffers) and shaders
- Replace textures, meshes and shaders
- Automatic (but limited) texture compression and decompression for replacing and extracting
- Automatic shader compilation (with ``glslang``) for replacing
- Rule based language for custom behaviour
	- Interprocess communication over sockets (``socket`` and ``write`` actions)
	- Verbose information about selected draw calls (``verbose`` action)
	- On-the-fly replacment of textures and meshes (``overload`` and ``preload`` actions)
	- Dump aspects of the framebuffer after selected draw calls (``dumpfb`` action)
	- Selection of draw calls (and also other things) by uses textures, meshes and shaders (``draw`` selector and ``with`` condition)
	- Modification of pipeline parameters at pipeline creation (``override`` action)
	- Runtime reflection into Vulkan structs
	- Data procession (kinda experiment) supported for a few conditions and actions (e.g. handling data received via IPC)
- Plugin interface for custom conditions and actions

## Required libraries
| Library | Reason | Inclusion |
| ------- | ------ | --------- |
| [Vulkan](https://www.vulkan.org/)  | It is a Vulkan layer after all. | Must be installed manually on the system. |
| [OpenSSL](https://www.openssl.org/) | Hashing data for easier identification. | Must be installed manually on the system. |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Automatic decompiling of shaders on extraction. | Submodule [``external/``](../external/)``SPIRV-Cross`` |
| [glslang](https://github.com/KhronosGroup/glslang) | Automatic shader compilation for replacing. | Submodule [``external/``](../external/)``glslang`` |
| [stb](https://github.com/nothings/stb) | Image loading and saving. | Submodule [``external/``](../external/)``stb`` |
| [Vulkan-ValidationLayers](https://github.com/KhronosGroup/Vulkan-ValidationLayers) | We need one source file of it for determining the size of images. | Submodule [``external/``](../external/)``Vulkan-ValidationLayers`` |
| [image_tools](../image_tools/) | Automatic (de)compression of images. | Part of *cheeky-imp* |
| [Doxygen](https://www.doxygen.nl) | Generating documentation | Must be installed manually on the system (*optional*). |
| [exprtk](http://www.partow.net/programming/exprtk/) | ``math`` data | Submodule [``external/``](../external/)``exprtk`` |
