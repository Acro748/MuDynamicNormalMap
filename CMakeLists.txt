cmake_minimum_required(VERSION 3.21)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

########################################################################################################################
## Define project
########################################################################################################################
project(
        MuDynamicNormalMap
        VERSION 1.0.0
        DESCRIPTION "MuDynamicNormalMap by Acro."
        LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

include(GNUInstallDirs)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY
)

set(headers
        include/Utility.hpp
        include/nifUtil.h
        include/Config.h
        include/PapyrusScripts.h
        include/ShaderManager.h
        include/ObjectNormalMapUpdater.h
        include/TaskManager.h
        include/Hook.h
        include/HookEvent.h
        include/Store.h
        include/Common.h
        include/Geometry.h
        include/InputManager.h
        include/RGBA.h
        include/ThreadPool.h
        include/MiniBenchMark.h
        include/ActorVertexHasher.h
        include/Condition.h
        include/Interface/Interface.h
        include/Interface/InterfaceImpl.h
)

set(sources
        src/nifUtil.cpp
        src/Config.cpp
        src/PapyrusScripts.cpp
        src/ShaderManager.cpp
        src/ObjectNormalMapUpdater.cpp
        src/TaskManager.cpp
        src/Hook.cpp
        src/HookEvent.cpp
        src/Store.cpp
        src/Geometry.cpp
        src/InputManager.cpp
        src/ActorVertexHasher.cpp
        src/Condition.cpp
        src/ThreadPool.cpp
        src/Main.cpp

        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

source_group(
        TREE ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        ${headers}
        ${sources})

########################################################################################################################
## Configure target DLL
########################################################################################################################
find_package(CommonLibSSE REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(JsonCpp CONFIG REQUIRED)
find_package(Directxtk CONFIG REQUIRED)
find_package(DirectXTex CONFIG REQUIRED)
find_package(detours)
find_package(xxhash CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${headers} ${sources})
add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_compile_features(
    ${PROJECT_NAME}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
        SKSE_SUPPORT_XBYAK
        _UNICODE
        UNICODE
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shader>
        $<INSTALL_INTERFACE:src>
        ${DETOURS_INCLUDE_DIRS}
)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ExtraRE/include>
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
        CommonLibSSE::CommonLibSSE
    PRIVATE
        magic_enum::magic_enum
        JsonCpp::JsonCpp
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows-static-md/lib/xxhash.lib>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows-static-md/lib/detours.lib>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows-static-md/lib/DirectXTex.lib>
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        include/PCH.h)
        
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/sdl;/utf-8;/Zi;/permissive-;/Zc:preprocessor;/Zc:inline;/JMC-;/O2;/fp:fast;/Ob3>")
target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")

install(TARGETS ${PROJECT_NAME}
        DESTINATION "${CMAKE_INSTALL_LIBDIR}")

########################################################################################################################
## Shader Script
########################################################################################################################
set(compute_shader_sources 
   ${CMAKE_CURRENT_SOURCE_DIR}/shader/UpdateObjectNormalMap.hlsl
   ${CMAKE_CURRENT_SOURCE_DIR}/shader/BleedTexture.hlsl
)
set(vertex_shader_sources 

)
set(pixel_shader_sources 

)

add_custom_target(ValidateShaders ALL 
    COMMENT "Running Shader Validation"
)
foreach(shader ${compute_shader_sources})
    get_filename_component(shader_name ${shader} NAME_WE)
    set(compiledShader "${CMAKE_CURRENT_SOURCE_DIR}/Shader/Compiled/CS_${shader_name}.cso")

    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        TARGET ValidateShaders
        COMMAND fxc.exe /T cs_5_0 /E CSMain /Fo ${compiledShader} /nologo ${shader}
        COMMAND ${CMAKE_COMMAND} -E echo "Shader valid : ${shader}"
    )
endforeach()
foreach(shader ${vertex_shader_sources})
    get_filename_component(shader_name ${shader} NAME_WE)
    set(compiledShader "${CMAKE_CURRENT_SOURCE_DIR}/Shader/Compiled/VS_${shader_name}.cso")

    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        TARGET ValidateShaders
        COMMAND fxc.exe /T vs_5_0 /E VSMain /Fo ${compiledShader} /nologo ${shader}
        COMMAND ${CMAKE_COMMAND} -E echo "Shader valid : ${shader}"
    )
endforeach()
foreach(shader ${pixel_shader_sources})
    get_filename_component(shader_name ${shader} NAME_WE)
    set(compiledShader "${CMAKE_CURRENT_SOURCE_DIR}/Shader/Compiled/PS_${shader_name}.cso")

    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        TARGET ValidateShaders
        COMMAND fxc.exe /T ps_5_0 /E PSMain /Fo ${compiledShader} /nologo ${shader}
        COMMAND ${CMAKE_COMMAND} -E echo "Shader valid : ${shader}"
    )
endforeach()

########################################################################################################################
## Automatic plugin deployment
########################################################################################################################
# Automatic deployment to FOMOD directory.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_NAME "Debug")
else()
    set(BUILD_NAME "Release")
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution/Plugin${BUILD_NAME}/")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution/Plugin${BUILD_NAME}/")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution/Plugin${BUILD_NAME}/")

file(GLOB_RECURSE OUTPUT_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution/**/*.dll")
file(GLOB_RECURSE OUTPUT_PDBS "${CMAKE_CURRENT_SOURCE_DIR}/contrib/Distribution/**/*.pdb")
set_property(TARGET ${PROJECT_NAME}
        APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${OUTPUT_DLLS}")
set_property(TARGET ${PROJECT_NAME}
        APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${OUTPUT_PDBS}")
