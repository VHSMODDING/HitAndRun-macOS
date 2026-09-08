cmake_minimum_required(VERSION 3.24)

project(HitAndRunMac LANGUAGES CXX OBJCXX)

find_package(PNG REQUIRED)

if(NOT APPLE)
    message(FATAL_ERROR "This port targets native macOS only.")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum supported macOS version" FORCE)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Native Apple Silicon architecture")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hitandrun_macos_input STATIC
    port/macos/input/MacInput.mm
    port/macos/input/MacRadController.cpp
    port/macos/input/MacControlProfile.mm
)

target_include_directories(hitandrun_macos_input PUBLIC port/macos/input)
target_include_directories(hitandrun_macos_input PRIVATE
    upstream/game/code
    upstream/game/libs/radcore/inc
)
target_compile_definitions(hitandrun_macos_input PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
)
target_link_libraries(hitandrun_macos_input PUBLIC
    "-framework Foundation"
    "-framework GameController"
)

add_library(hitandrun_macos_foundation STATIC
    upstream/game/libs/radcore/src/radtime/time.cpp
    port/macos/foundation/MacDougHeap.cpp
    port/macos/foundation/MacRadDiagnostics.cpp
    port/macos/foundation/MacGameData.cpp
    port/macos/foundation/MacRadThread.cpp
    port/macos/foundation/MacRadDebug.cpp
    upstream/game/libs/radcore/src/radobject/object.cpp
    upstream/game/libs/radcore/src/radobjectbtree/objectbtree.cpp
    upstream/game/libs/radcore/src/radmemory/align.cpp
    upstream/game/libs/radcore/src/radmemory/platalloc.cpp
    upstream/game/libs/radcore/src/radmemory/memorymanager.cpp
    upstream/game/libs/radcore/src/radmemory/memoryspacewin32.cpp
    upstream/game/libs/radcore/src/radmemory/memorypool.cpp
    upstream/game/libs/radcore/src/radmemory/binallocator.cpp
    upstream/game/libs/radcore/src/radmemory/externalmemoryheap.cpp
    upstream/game/libs/radcore/src/radmemory/externalmemoryobject.cpp
    upstream/game/libs/radcore/src/radmemory/memoryobject.cpp
    upstream/game/libs/radcore/src/radmemory/staticheap.cpp
    upstream/game/libs/radcore/src/radmemory/trackingheap.cpp
    upstream/game/libs/radcore/src/radstring/string.cpp
    upstream/game/libs/radcore/src/radkey/radkey.cpp
)
target_include_directories(hitandrun_macos_foundation PRIVATE
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcore/src
    upstream/game/libs/radcore/src/pch
    upstream/game/libs/radcore/src/radmemory
)
target_compile_definitions(hitandrun_macos_foundation PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
)
target_compile_definitions(hitandrun_macos_foundation PUBLIC
    HMR_DEFAULT_DATA_ROOT="${CMAKE_SOURCE_DIR}/upstream/game/PC"
)

add_library(hitandrun_macos_file_runtime STATIC
    upstream/game/libs/radcore/src/radfile/macos/macdrive.cpp
    upstream/game/libs/radcore/src/radfile/common/platformdrives.cpp
    upstream/game/libs/radcore/src/radfile/common/requests.cpp
    upstream/game/libs/radcore/src/radfile/common/file.cpp
    upstream/game/libs/radcore/src/radfile/common/filesystem.cpp
    upstream/game/libs/radcore/src/radfile/common/radfile.cpp
    upstream/game/libs/radcore/src/radfile/common/drive.cpp
    upstream/game/libs/radcore/src/radfile/common/drivethread.cpp
    upstream/game/libs/radcore/src/radfile/common/instancedrive.cpp
    upstream/game/libs/radcore/src/radfile/common/filecache.cpp
    upstream/game/libs/radcore/src/radfile/common/cementer.cpp
)
target_include_directories(hitandrun_macos_file_runtime PRIVATE
    port/macos/foundation
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcore/src
    upstream/game/libs/radcore/src/pch
    upstream/game/libs/radcore/src/radfile/common
)
target_compile_definitions(hitandrun_macos_file_runtime PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
    HMR_DEFAULT_DATA_ROOT="${CMAKE_SOURCE_DIR}/upstream/game/PC"
)
target_link_libraries(hitandrun_macos_file_runtime PUBLIC hitandrun_macos_foundation)

# RadLoad drives asynchronous P3D and game-data requests.  Its source is
# platform-neutral once backed by the native RadFile implementation above.
file(GLOB HMR_RADLOAD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radcontent/src/radload/*.cpp"
)
add_library(hitandrun_radload_macos STATIC ${HMR_RADLOAD_SOURCES})
target_include_directories(hitandrun_radload_macos PRIVATE
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radcontent/src
    upstream/game/libs/radmath
)
target_compile_definitions(hitandrun_radload_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_link_libraries(hitandrun_radload_macos PUBLIC
    hitandrun_macos_file_runtime
    hitandrun_macos_foundation
    hitandrun_radmath
)

# Decoder, streaming and mixer-independent RadSound code.  Platform voice
# output is intentionally separate so the forthcoming CoreAudio HAL can be
# implemented without carrying DirectSound into the macOS build.
file(GLOB_RECURSE HMR_RADSOUND_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radsound/src/common/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radsound/src/math/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radsound/src/radsound/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radsound/src/hal/common/*.cpp"
)
add_library(hitandrun_radsound_core_macos STATIC EXCLUDE_FROM_ALL ${HMR_RADSOUND_CORE_SOURCES})
target_include_directories(hitandrun_radsound_core_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radcontent/src
    upstream/game/libs/radmath
    upstream/game/libs/radsound/inc
    upstream/game/libs/radsound/src
    upstream/game/libs/radsound/src/pch
    upstream/game/libs/radsound/src/common
)
target_compile_definitions(hitandrun_radsound_core_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_compile_options(hitandrun_radsound_core_macos PRIVATE "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h")
target_link_libraries(hitandrun_radsound_core_macos PUBLIC
    hitandrun_radload_macos
    hitandrun_macos_foundation
    hitandrun_macos_file_runtime
    hitandrun_radmath
)

add_library(hitandrun_radsound_hal_macos STATIC EXCLUDE_FROM_ALL
    port/macos/sound/MacRadSoundHal.cpp
)
target_include_directories(hitandrun_radsound_hal_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radsound/inc
    upstream/game/libs/radsound/src
    upstream/game/libs/radsound/src/common
    upstream/game/libs/radsound/src/hal/common
)
target_compile_definitions(hitandrun_radsound_hal_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_compile_options(hitandrun_radsound_hal_macos PRIVATE "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h")
target_link_libraries(hitandrun_radsound_hal_macos PUBLIC hitandrun_radsound_core_macos hitandrun_macos_foundation "-framework AudioToolbox")

# Interactive-music sequencing sits above RadSound and is independent of the
# eventual CoreAudio voice implementation.
file(GLOB_RECURSE HMR_RADMUSIC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/ods/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/radmusic/framework/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/radmusic/music/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/radmusic/resource/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/radmusic/schema/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/radmusic/sequence/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radmusic/src/memory/*.cpp"
)
add_library(hitandrun_radmusic_macos STATIC EXCLUDE_FROM_ALL ${HMR_RADMUSIC_SOURCES})
target_include_directories(hitandrun_radmusic_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radmath
    upstream/game/libs/radsound/inc
    upstream/game/libs/radsound/src/common
    upstream/game/libs/radmusic/inc
    upstream/game/libs/radmusic/src
    upstream/game/libs/radmusic/src/pch
)
target_compile_definitions(hitandrun_radmusic_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_compile_options(hitandrun_radmusic_macos PRIVATE "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h")
target_link_libraries(hitandrun_radmusic_macos PUBLIC
    hitandrun_radsound_core_macos
    hitandrun_radsound_hal_macos
    hitandrun_macos_foundation
    hitandrun_radload_macos
)

# The original renderer shipped an OpenGL implementation for Linux.  This
# target is its macOS compatibility baseline; Metal presentation is owned by
# the Cocoa host while Pure3D rendering calls are brought up incrementally.
add_library(hitandrun_pure3d_gl_macos STATIC
    upstream/game/libs/pure3d/pddi/base/basecontext.cpp
    upstream/game/libs/pure3d/pddi/base/baseshader.cpp
    upstream/game/libs/pure3d/pddi/base/font.cpp
    upstream/game/libs/pure3d/pddi/base/pddiobj.cpp
    upstream/game/libs/pure3d/pddi/gl/glcon.cpp
    upstream/game/libs/pure3d/pddi/gl/gldev.cpp
    upstream/game/libs/pure3d/pddi/gl/glmat.cpp
    upstream/game/libs/pure3d/pddi/gl/gltex.cpp
    upstream/game/libs/pure3d/pddi/gl/display_macos/gldisplay.mm
)
target_include_directories(hitandrun_pure3d_gl_macos PRIVATE
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/radmath
    upstream/game/libs/radcore/inc
)
target_compile_definitions(hitandrun_pure3d_gl_macos PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
    PDDI_NO_ASM
)
target_link_libraries(hitandrun_pure3d_gl_macos PUBLIC
    "-framework AppKit"
    "-framework OpenGL"
)

add_library(hitandrun_radmath STATIC
    upstream/game/libs/radmath/radmath/radmath.cpp
    upstream/game/libs/radmath/radmath/matrix.cpp
    upstream/game/libs/radmath/radmath/vector.cpp
    upstream/game/libs/radmath/radmath/quaternion.cpp
    upstream/game/libs/radmath/radmath/plane.cpp
    upstream/game/libs/radmath/radmath/triangle.cpp
    upstream/game/libs/radmath/radmath/trig.cpp
    upstream/game/libs/radmath/radmath/numerical.cpp
    upstream/game/libs/radmath/radmath/random.cpp
    upstream/game/libs/radmath/radmath/spline.cpp
)
target_include_directories(hitandrun_radmath PUBLIC upstream/game/libs/radmath)
target_compile_definitions(hitandrun_radmath PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_link_libraries(hitandrun_pure3d_gl_macos PUBLIC hitandrun_radmath)

# Pure3D scene, animation and effects runtime used by the PC assets.
file(GLOB HMR_P3D_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/anim/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/effects/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/scenegraph/*.cpp"
)
# The matching public header deliberately disables the abandoned stencil-shadow
# implementation; keeping its orphaned translation unit in a modern build
# therefore produces definitions for classes that do not exist.
list(REMOVE_ITEM HMR_P3D_RUNTIME_SOURCES
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/shadow.cpp"
)
add_library(hitandrun_pure3d_runtime_core STATIC
    ${HMR_P3D_RUNTIME_SOURCES}
    upstream/game/libs/pure3d/p3d/platform/macos/platform.cpp
)
target_include_directories(hitandrun_pure3d_runtime_core PRIVATE
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/radmath
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
)
target_compile_definitions(hitandrun_pure3d_runtime_core PRIVATE
    RAD_MACOS RAD_PC RAD_RELEASE PDDI_NO_ASM
)
target_link_libraries(hitandrun_pure3d_runtime_core PUBLIC
    hitandrun_pure3d_gl_macos
    hitandrun_macos_foundation
    hitandrun_radload_macos
    PNG::PNG
)

# Legacy mapper code is platform-neutral once its DirectInput identifiers are
# treated as persisted values. Compile it continuously on macOS while the
# IRadController backend is being replaced.
add_library(hitandrun_input_mapping OBJECT
    upstream/game/code/input/Gamepad.cpp
    upstream/game/code/input/Keyboard.cpp
    upstream/game/code/input/Mouse.cpp
    upstream/game/code/input/SteeringWheel.cpp
    upstream/game/code/input/RealController.cpp
    upstream/game/code/input/button.cpp
    upstream/game/code/input/mapper.cpp
    upstream/game/code/input/mappable.cpp
    port/macos/input/MacLegacyRumble.cpp
    upstream/game/code/input/virtualinputs.cpp
    upstream/game/code/input/usercontrollerWin32.cpp
    upstream/game/code/input/inputmanager.cpp
)
target_include_directories(hitandrun_input_mapping PRIVATE
    upstream/game/code
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radmath
    upstream/game/libs/pure3d
    upstream/game/libs/radcontent/inc
)
target_compile_definitions(hitandrun_input_mapping PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
)

# Compile the original game code as a distinct macOS porting target.  The
# platform entry points remain separate because they are tied directly to the
# Win32/console event loops; all portable gameplay code is kept under compiler
# coverage while those replacements are implemented.
file(GLOB_RECURSE HMR_GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/code/*.cpp"
)
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/main/(win32main|win32platform|ps2main|ps2platform|gcmain|gcplatform|xboxmain|xboxplatform)\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/main/gamecube_extras/")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/main/all(ps2|gc)main\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/all[^/]*\\.cpp$")
# DirectInput/force-feedback units are replaced by the native GameController
# backend and are built separately above while their game-facing adapter is
# completed.
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/input/")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/fmvplayer/")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/gui/frontend/guiscreencontrollerWin32(old)?\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/gui/frontend/guiscreenmultichoosechar\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/gui/frontend/guiscreenmultisetup\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/gui/guiscreenmessage\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/presentation/gui/ingame/guiscreenhudmap\\.cpp$")
list(FILTER HMR_GAME_SOURCES EXCLUDE REGEX "/sound/soundfx/(gc|ps2|xbox|win32)reverbcontroller\\.cpp$")
add_library(hitandrun_game_code_macos STATIC EXCLUDE_FROM_ALL
    ${HMR_GAME_SOURCES}
    upstream/game/code/render/DSG/IntersectDSG.cpp
    upstream/game/code/render/DSG/LensFlareDSG.cpp
    port/macos/game/MacFMVPlayer.cpp
    port/macos/game/MacMovieOverlay.mm
    upstream/game/code/presentation/fmvplayer/fmvuserinputhandler.cpp
    port/macos/game/MacGuiScreenMessage.cpp
    port/macos/game/MacLegacyGlobals.cpp
    port/macos/sound/MacReverbController.cpp
    port/macos/game/MacGamePlatform.cpp
)
target_include_directories(hitandrun_game_code_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/pure3d/p3d
    upstream/game/libs/radcore/inc
    upstream/game/libs/radmath
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radscript/inc
    upstream/game/libs/radsound/inc
    upstream/game/libs/radmusic/inc
    upstream/game/libs/radmovie/inc
    upstream/game/libs/scrooby/inc
    upstream/game/libs/scrooby/src
    upstream/game/libs/sim
    upstream/game/libs/poser/inc
    upstream/game/libs/choreo/inc
    upstream/game/libs/sim
)
target_compile_definitions(hitandrun_game_code_macos PRIVATE
    RAD_MACOS RAD_PC RAD_RELEASE PDDI_NO_ASM
)
target_compile_options(hitandrun_game_code_macos PRIVATE
    "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/utility.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/anim/multicontroller.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/sprite.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/camera.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/pointcamera.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/view.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/matrixstack.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/anim/compositedrawable.hpp"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/unicode.hpp"
)
target_link_libraries(hitandrun_game_code_macos PUBLIC
    hitandrun_pure3d_runtime_core
    hitandrun_macos_foundation
    hitandrun_macos_file_runtime
    hitandrun_input_mapping
)

# Runtime script interpreter used by mission and presentation data.  The PC
# implementation supplies an x86 dynamic-call thunk; the source target below
# makes the remaining arm64 work visible independently of the game archive.
file(GLOB_RECURSE HMR_RADSCRIPT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/radscript/src/*.cpp"
)
list(FILTER HMR_RADSCRIPT_SOURCES EXCLUDE REGEX "/(win32|ps2|gcn)/")
add_library(hitandrun_radscript_macos STATIC EXCLUDE_FROM_ALL
    ${HMR_RADSCRIPT_SOURCES}
)
target_include_directories(hitandrun_radscript_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/radcore/inc
    upstream/game/libs/radscript/inc
    upstream/game/libs/radscript/src
    upstream/game/libs/radscript/src/pch
)
target_compile_definitions(hitandrun_radscript_macos PRIVATE
    RAD_MACOS RAD_PC RAD_RELEASE
)
target_compile_options(hitandrun_radscript_macos PRIVATE
    "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h"
)
target_link_libraries(hitandrun_radscript_macos PUBLIC hitandrun_macos_foundation)

# Front-end UI runtime used by the original Scrooby projects in the PC data.
file(GLOB_RECURSE HMR_SCROOBY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/scrooby/src/*.cpp"
)
list(FILTER HMR_SCROOBY_SOURCES EXCLUDE REGEX "/toollib/")
# Authoring-only localization database code depends on the removed Excel/GFE
# toolchain; runtime translation is provided by GTxlate/GfeXlate.
list(FILTER HMR_SCROOBY_SOURCES EXCLUDE REGEX "/localization/(languageDatabase|languageEntry)\\.cpp$")
# Pure3D already supplies this allocator implementation to the runtime.
list(FILTER HMR_SCROOBY_SOURCES EXCLUDE REGEX "/utility/memory\\.cpp$")
add_library(hitandrun_scrooby_macos STATIC EXCLUDE_FROM_ALL ${HMR_SCROOBY_SOURCES})
target_include_directories(hitandrun_scrooby_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    port/macos/legacy
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/pure3d/p3d
    upstream/game/libs/radcore/inc
    upstream/game/libs/radmath
    upstream/game/libs/radcontent/inc
    upstream/game/libs/scrooby/inc
    upstream/game/libs/scrooby/src
)
target_compile_definitions(hitandrun_scrooby_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE PDDI_NO_ASM)
target_compile_options(hitandrun_scrooby_macos PRIVATE
    "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h"
    "-include${CMAKE_SOURCE_DIR}/port/macos/ScroobyMemoryCompat.h"
    "-include${CMAKE_SOURCE_DIR}/upstream/game/libs/pure3d/p3d/utility.hpp"
)
target_link_libraries(hitandrun_scrooby_macos PUBLIC
    hitandrun_pure3d_runtime_core
    hitandrun_macos_file_runtime
    hitandrun_macos_foundation
)

file(GLOB HMR_POSER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/upstream/game/libs/poser/src/*.cpp")
add_library(hitandrun_poser_macos STATIC EXCLUDE_FROM_ALL ${HMR_POSER_SOURCES})
target_include_directories(hitandrun_poser_macos PRIVATE
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/radcore/inc
    upstream/game/libs/radmath
    upstream/game/libs/radcontent/inc
    upstream/game/libs/pure3d
    upstream/game/libs/poser/inc
)
target_compile_definitions(hitandrun_poser_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_link_libraries(hitandrun_poser_macos PUBLIC hitandrun_radmath hitandrun_pure3d_runtime_core)

file(GLOB HMR_CHOREO_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/upstream/game/libs/choreo/src/*.cpp")
add_library(hitandrun_choreo_macos STATIC EXCLUDE_FROM_ALL ${HMR_CHOREO_SOURCES})
target_include_directories(hitandrun_choreo_macos PRIVATE
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/radcore/inc
    upstream/game/libs/radmath
    upstream/game/libs/radcontent/inc
    upstream/game/libs/pure3d
    upstream/game/libs/poser/inc
    upstream/game/libs/choreo/inc
    upstream/game/libs/sim
)
target_compile_definitions(hitandrun_choreo_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
target_compile_options(hitandrun_choreo_macos PRIVATE "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h")
target_link_libraries(hitandrun_choreo_macos PUBLIC
    hitandrun_poser_macos
    hitandrun_pure3d_runtime_core
    hitandrun_macos_foundation
)

# Physics and collision runtime referenced by vehicles, characters and world
# loading.  Build the original portable simulation sources on arm64 rather
# than falling back to the Windows archive.
file(GLOB_RECURSE HMR_SIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simcommon/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simphysics/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simcollision/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simflexible/*.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simik/*.cpp"
)
# Historical alternate implementation duplicated FlexibleObject and was not
# part of the production simulation library; it also lacks its namespace.
list(REMOVE_ITEM HMR_SIM_SOURCES
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simflexible/flexibleobject1.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simflexible/particlesyst3d.cpp"
    "${CMAKE_SOURCE_DIR}/upstream/game/libs/sim/simflexible/psystongeom1d.cpp"
)
add_library(hitandrun_sim_macos STATIC EXCLUDE_FROM_ALL ${HMR_SIM_SOURCES})
set_target_properties(hitandrun_sim_macos PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
target_include_directories(hitandrun_sim_macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radmath
    upstream/game/libs/pure3d
    upstream/game/libs/sim
    upstream/game/libs/poser/inc
)
target_compile_definitions(hitandrun_sim_macos PRIVATE RAD_MACOS RAD_PC RAD_RELEASE PDDI_NO_ASM)
target_compile_options(hitandrun_sim_macos PRIVATE "-include${CMAKE_SOURCE_DIR}/port/macos/LegacyRef.h")
target_link_libraries(hitandrun_sim_macos PUBLIC
    hitandrun_radmath
    hitandrun_pure3d_runtime_core
    hitandrun_poser_macos
    hitandrun_macos_foundation
)
target_link_libraries(hitandrun_game_code_macos PUBLIC hitandrun_sim_macos)

# Keep the full-game link audit separate from the usable Cocoa host while the
# last PC-only runtime services are replaced.  Building this target reveals
# real missing implementation symbols instead of hiding them in archives.
add_executable(hitandrun_game_link_probe EXCLUDE_FROM_ALL
    port/macos/game/MacGameBootstrap.cpp
    port/macos/game/MacGameLinkProbe.cpp
)
target_include_directories(hitandrun_game_link_probe PRIVATE
    ${CMAKE_SOURCE_DIR}
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
    upstream/game/libs/radscript/inc
    upstream/game/libs/radsound/inc
    upstream/game/libs/radmusic/inc
    upstream/game/libs/scrooby/inc
    upstream/game/libs/sim
    upstream/game/libs/poser/inc
    upstream/game/libs/choreo/inc
)
target_compile_definitions(hitandrun_game_link_probe PRIVATE RAD_MACOS RAD_PC RAD_RELEASE PDDI_NO_ASM)
target_link_libraries(hitandrun_game_link_probe PRIVATE
    hitandrun_game_code_macos
    hitandrun_radscript_macos
    hitandrun_scrooby_macos
    hitandrun_choreo_macos
    hitandrun_sim_macos
    hitandrun_radsound_core_macos
    hitandrun_radmusic_macos
    hitandrun_macos_input
    hitandrun_macos_foundation
    hitandrun_macos_file_runtime
    hitandrun_radload_macos
    hitandrun_pure3d_runtime_core
    hitandrun_pure3d_gl_macos
    "-framework AppKit"
    "-framework AVFoundation"
    "-framework AVKit"
    "-framework CoreMedia"
    "-framework OpenGL"
)

add_executable(HitAndRunPortHost MACOSX_BUNDLE
    port/macos/host/MacHost.mm
    port/macos/game/MacGameBootstrap.cpp
    upstream/game/code/main/tuidunaligned.cpp
    assets/icons/AppIcon.icns
)
set_source_files_properties(assets/icons/AppIcon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
target_include_directories(HitAndRunPortHost PRIVATE port/macos/foundation)
target_include_directories(HitAndRunPortHost PRIVATE
    upstream/game/code
    upstream/game/libs
    upstream/game/libs/pure3d
    upstream/game/libs/radmath
    upstream/game/libs/radcore/inc
    upstream/game/libs/radcontent/inc
)
target_compile_definitions(HitAndRunPortHost PRIVATE
    RAD_MACOS
    RAD_PC
    RAD_RELEASE
    PDDI_NO_ASM
)

target_link_libraries(HitAndRunPortHost PRIVATE
    hitandrun_game_code_macos
    hitandrun_radscript_macos
    hitandrun_scrooby_macos
    hitandrun_choreo_macos
    hitandrun_sim_macos
    hitandrun_radsound_hal_macos
    hitandrun_radsound_core_macos
    hitandrun_radmusic_macos
    hitandrun_input_mapping
    hitandrun_macos_file_runtime
    hitandrun_radload_macos
    hitandrun_macos_input
    hitandrun_macos_foundation
    hitandrun_pure3d_gl_macos
    hitandrun_pure3d_runtime_core
    "-framework AudioToolbox"
    "-framework AppKit"
    "-framework AVFoundation"
    "-framework AVKit"
    "-framework CoreMedia"
    "-framework OpenGL"
)

set_target_properties(HitAndRunPortHost PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.hitandrun.porthost"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    # CMake writes this value directly into Info.plist; XML requires '&' to
    # be escaped or LaunchServices rejects the bundle before main() runs.
    MACOSX_BUNDLE_BUNDLE_NAME "Hit &amp; Run Port Host"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.5.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "500"
)

# A built application must be self-contained: its licensed PC game data is
# copied as part of the host build, not only by an optional staging command.
add_custom_command(TARGET HitAndRunPortHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:HitAndRunPortHost>/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/upstream/game/PC"
        "$<TARGET_BUNDLE_DIR:HitAndRunPortHost>/Contents/Resources/GameData"
    COMMENT "Embedding PC game data in HitAndRunPortHost.app"
    VERBATIM
)

# Opt-in distribution target: embeds the legally owned PC data in the app so
# the resulting bundle is independent of the source-tree path.
add_custom_target(stage_game_data
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/HitAndRunPortHost.app/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/upstream/game/PC"
        "${CMAKE_BINARY_DIR}/HitAndRunPortHost.app/Contents/Resources/GameData"
    DEPENDS HitAndRunPortHost
    COMMENT "Embedding PC game data in HitAndRunPortHost.app"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_executable(macos_audio_hal_smoke_test
        port/macos/sound/MacAudioHalSmoke.cpp
    )
    target_include_directories(macos_audio_hal_smoke_test PRIVATE
        upstream/game/code
        upstream/game/libs/radcore/inc
        upstream/game/libs/radsound/inc
    )
    target_compile_definitions(macos_audio_hal_smoke_test PRIVATE RAD_MACOS RAD_PC RAD_RELEASE)
    target_link_libraries(macos_audio_hal_smoke_test PRIVATE hitandrun_radsound_hal_macos)
    add_test(NAME macos_audio_hal_smoke COMMAND macos_audio_hal_smoke_test)

    add_executable(macos_controller_smoke_test
        port/macos/input/MacControllerSmoke.cpp
    )
    target_include_directories(macos_controller_smoke_test PRIVATE
        upstream/game/code
        upstream/game/libs/radcore/inc
    )
    target_compile_definitions(macos_controller_smoke_test PRIVATE
        RAD_MACOS
        RAD_PC
        RAD_RELEASE
    )
    target_link_libraries(macos_controller_smoke_test PRIVATE hitandrun_macos_input hitandrun_macos_foundation)
    add_test(NAME macos_controller_smoke COMMAND macos_controller_smoke_test)

    add_executable(macos_file_smoke_test
        port/macos/foundation/MacFileSmoke.cpp
    )
    target_include_directories(macos_file_smoke_test PRIVATE
        upstream/game/code
        upstream/game/libs/radcore/inc
    )
    target_compile_definitions(macos_file_smoke_test PRIVATE
        RAD_MACOS
        RAD_PC
        RAD_RELEASE
    )
    target_link_libraries(macos_file_smoke_test PRIVATE
        hitandrun_macos_file_runtime
        hitandrun_macos_foundation
    )
    add_test(NAME macos_file_smoke COMMAND macos_file_smoke_test)

    add_executable(macos_pddi_smoke_test
        port/macos/host/MacPddiSmoke.cpp
    )
    target_include_directories(macos_pddi_smoke_test PRIVATE
        upstream/game/libs
        upstream/game/libs/pure3d
        upstream/game/libs/radmath
        upstream/game/libs/radcore/inc
    )
    target_compile_definitions(macos_pddi_smoke_test PRIVATE
        RAD_MACOS
        RAD_PC
        RAD_RELEASE
        PDDI_NO_ASM
    )
    target_link_libraries(macos_pddi_smoke_test PRIVATE
        hitandrun_pure3d_gl_macos
        "-framework OpenGL"
    )
    add_test(NAME macos_pddi_smoke COMMAND macos_pddi_smoke_test)
endif()

# This host owns the macOS event loop. The original Win32 game is not linked
# yet: its DirectInput and Win32 window layers still have to be replaced.
