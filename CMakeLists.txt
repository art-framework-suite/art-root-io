# ======================================================================
#  art_root_io main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/art_root_io/ups/setup_for_development
#  cmake [-DCMAKE_INSTALL_PREFIX=/install/path]
#        -DCMAKE_BUILD_TYPE=$CETPKG_TYPE
#        $CETPKG_SOURCE
#  make
#  make test
#  make install
#  make package (builds distribution tarfile)
# ======================================================================


# use cmake 3.11 or later
cmake_minimum_required (VERSION 3.12...3.14 FATAL_ERROR)

project(art_root_io VERSION 1.02.01 LANGUAGES C CXX)

# cetmodules contains our cmake modules
find_package(cetmodules 1.01.01 REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic
  EXTRA_CXX_FLAGS -Wnon-virtual-dtor -Wdelete-non-virtual-dtor -DBOOST_TEST_NO_OLD_TOOLS)

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_package(hep_concurrency REQUIRED)
find_package(cetlib_except REQUIRED)
find_package(cetlib REQUIRED)
find_package(fhiclcpp REQUIRED)
find_package(messagefacility REQUIRED)
find_package(canvas REQUIRED)
find_package(canvas_root_io REQUIRED)
find_package(art REQUIRED)
find_package(ROOT REQUIRED COMPONENTS)
find_package(Boost REQUIRED COMPONENTS date_time filesystem program_options)

# SQLite
cet_find_library(SQLITE3 NAMES sqlite3 REQUIRED)

# macros for making art libraries/dictionaries, etc. and plugins
include(ArtMake)
include(BuildPlugins)

# source
add_subdirectory(art_root_io)

# ups - table and config files
add_subdirectory(cmakeConfig)

# tools - migration scripts
add_subdirectory(tools)
