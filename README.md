# cheeky-imp
A set of tools for hooking into Vulkan games and replacing shaders, textures, and meshes.

*This README.md is very work-in-progress, so please excuse the shortness and directness.*

The tools are mainly made for Linux and games that use DXVK (or Vulkan natively).
Some (``mesh_buffer_tools`` and ``image_tools``) should also work on Windows,
but I am not so sure about the ``vulkan_layer``.

I will try to create a ``README.md`` for each individual project (in the subdirectories),
but that might take quite a while.

***Always clone with ``--recurse-submodules`` as this repository uses multiple submodules which are vital for almost all subprojects.***

## Building

Most subprojects use *CMake*.
Just create a ``build/`` subdirectory, open *bash* in it and do ``cmake ..`` and ``make``.
