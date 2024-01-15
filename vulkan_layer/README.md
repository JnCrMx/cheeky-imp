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

## Usage

### Build the layer
This project uses the *CMake* build system.

**Make sure to *ALWAYS* build it in *Release* (or *RelWithDebInfo*) mode! Building in *Debug* mode will result in incredibly poor performance!**

You can use the following commands to download and build the Vulkan layer:
```bash
git clone --recurse-submodules https://git.jcm.re/cheeky-imp/cheeky-imp
cd vulkan_layer
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo --target all --
```

### Tell Vulkan where to find the layer
The easiest way to tell Vulkan where to find the layer is to make use of environment variables (see [here](https://vulkan.lunarg.com/doc/view/1.3.204.1/windows/layer_configuration.html) for more information on Vulkan Layer configuration):

- ``VK_LAYER_PATH`` needs to be pointed to ``/path/to/repository/vulkan_layer``. If you already set this variable, use colons (``:``) to separate multiple paths.
- ``VK_INSTANCE_LAYERS`` needs to contain ``VK_LAYER_CHEEKYIMP_CheekyLayer``, so Vulkan knows to automatically load the layer on instance creation. If you want to use multiple layers, use colons (``:``) to separate them. You might need to play around with the order a bit.

**Warning: Combining the layer with other layers (in particular RenderDoc) can result in instability and lots of crashes.**

### Configuring the layer
Sadly, the layer does not support Vulkan Layer Settings Files yet and uses its own kind of configuration files.
A path to the config needs to be provided with the ``CHEEKY_LAYER_CONFIG`` environment variable.
So, create a file (for example called ``config.txt``) and set ``CHEEKY_LAYER_CONFIG`` to the location of the file.

This file simply contains options in a ``key=value`` format on each line:
```
dump=false|false
dumpDirectory=/absolute/path/to/directory/for/dumping
override=true|false
overrideDirectory=/absolute/path/to/directory/for/overrides
logFile=/absolute/path/to/log/file.txt
ruleFile=/absolute/path/to/rule/file.txt
hookDraw=true|false
pluginDirectory=/absolute/path/to/plugin/directory
application=ApplicationName.exe
```

The following options are available:
| Option | Values | Required | Description |
|---|---|---|---|
|``dump``|``true`` or ``false``| yes | ``true`` if textures, buffers and shaders should be dumped when loaded. |
|``dumpDirectory``|absolute path| yes | Path to the directory to use for dumping, should contain ``images/``, ``buffers/``, ``shaders/`` subdirectories. |
|``override``|``true`` or ``false``| yes | ``true`` if textures, buffers and shaders should be potentially overridden when loaded. |
|``overrideDirectory``|absolute path| yes | Path to the directory to search for the images/buffers/shaders that are to be overridden, should contain ``images/``, ``buffers/``, ``shaders/`` subdirectories. |

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

## Adding custom logic with Rules

This Vulkan layer supports custom logic in the form of "rules".

The rules are store in the ``ruleFile``; usually ``rules.txt``.
