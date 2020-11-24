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
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

find_package(cetmodules 2.04.00 REQUIRED)
project(art_root_io VERSION 1.04.03 LANGUAGES CXX C)

include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic)

if(DEFINED ENV{CET_SANITIZE_ADDRESS} AND DEFINED ENV{GCC_FQ_DIR})
  find_file(ASAN_LIB libasan.so NO_DEFAULT_PATH PATHS $ENV{GCC_FQ_DIR}/lib64)
  cet_add_compiler_flags(CXX -fsanitize=address)
  cet_add_compiler_flags(C -fsanitize=address)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

if(DEFINED ENV{CET_SANITIZE_THREAD} AND DEFINED ENV{GCC_FQ_DIR})
  find_file(TSAN_LIB libtsan.so NO_DEFAULT_PATH PATHS $ENV{GCC_FQ_DIR}/lib64)
  cet_add_compiler_flags(CXX -fsanitize=thread -D__SANITIZE_THREAD__)
  cet_add_compiler_flags(C -fsanitize=thread -D__SANITIZE_THREAD__)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=thread -D__SANITIZE_THREAD__")
endif()

cet_report_compiler_flags(REPORT_THRESHOLD VERBOSE)

cet_find_package(art REQUIRED)
cet_find_package(canvas_root_io REQUIRED)
cet_find_package(canvas REQUIRED)
cet_find_package(messagefacility REQUIRED)
cet_find_package(fhiclcpp REQUIRED)
cet_find_package(cetlib REQUIRED)
cet_find_package(cetlib_except REQUIRED)
cet_find_package(CLHEP REQUIRED)
cet_find_package(hep_concurrency REQUIRED)
cet_find_package(SQLite3 REQUIRED)
cet_find_package(Boost COMPONENTS date_time program_options filesystem REQUIRED)
cet_find_package(ROOT REQUIRED)
cet_find_package(Catch2 REQUIRED PRIVATE)

# macros for making art libraries/dictionaries, etc. and plugins
include(ArtMake)
include(BuildPlugins)

# source
add_subdirectory(art_root_io)

# ups - table and config files

# tools - migration scripts
add_subdirectory(tools)

# packaging utility
cet_cmake_config(COMPATIBILITY SameMajorVersion)
