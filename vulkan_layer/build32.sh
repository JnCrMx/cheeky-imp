mkdir -p build32

cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_FIND_ROOT_PATH=/usr/lib32 -DVulkan_LIBRARY=/usr/lib32/libvulkan.so -S . -B build32/
cmake --build build32 --parallel
