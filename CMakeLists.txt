# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ZX2D10GE01R)

# ⚫ AJOUTÉ (25/10/2026) : pack d'images (animaux, décor, accessoires) généré par tools/assets (npm run gen),
# écrit dans la partition "assets" par idf.py flash (et inclus dans flash_args / firmware fusionné)
esptool_py_flash_to_partition(flash "assets" "${CMAKE_CURRENT_SOURCE_DIR}/assets/pet_assets.bin")
