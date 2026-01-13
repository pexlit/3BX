conan install . --output-folder=build --build=missing --settings=build_type=Debug

mkdir build
cd build

cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
ninja -j$(nproc)