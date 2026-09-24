cmake_minimum_required(VERSION 3.21)

project(ExtraDataExtender VERSION 1.0.0)

find_package(unofficial-lmdb CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)

set(CMAKE_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_LIB_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CPM)

CPMAddPackage("gh:stephenberry/glaze@8.3.0")

if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

add_compile_definitions(SKYRIM)
add_compile_definitions(SKYRIM_AE)
set(CommonLibPath "${CMAKE_LIB_DIR}/commonlibsse-ng")
set(CommonLibName "CommonLibSSE")
set(GameVersion "Skyrim")
include(${CommonLibPath}/cmake/CommonLibSSE.cmake)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

set(ENABLE_SKYRIM_SE ON)
set(ENABLE_SKYRIM_AE ON)
set(ENABLE_SKYRIM_VR OFF)
set(BUILD_TESTS OFF)
set(WITH_EXAMPLES OFF)

if(MSVC)
    add_compile_definitions(
        _UNICODE
    )

    if(NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
        add_compile_options(
            /MP # Build with Multiple Processes
        )
    endif()
endif()

add_subdirectory(${CommonLibPath} EXCLUDE_FROM_ALL)
add_subdirectory("${CMAKE_LIB_DIR}/emhash" EXCLUDE_FROM_ALL)

find_package(spdlog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")
find_package(unofficial-lmdb CONFIG REQUIRED)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SIMPLEINI_INCLUDE_DIRS}

)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.*")

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${SOURCES})
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE
  spdlog::spdlog
  imgui::imgui
  emhash
  glaze::glaze
  unofficial::lmdb::lmdb
)


# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER )
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")
    file(MAKE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

    # TODO: make this actually work with the res folder

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        # Ensure SKSE/Plugins directory exists in archive output
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/SKSE/Plugins"
        VERBATIM
    )

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            # Ensure res folder is copied into OUTPUT_FOLDER
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/res" "${OUTPUT_FOLDER}/${PROJECT_NAME}"
            VERBATIM
        )
    endif()

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/interface")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            # Ensure interface folder is copied into OUTPUT_FOLDER
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/interface" "${OUTPUT_FOLDER}/interface"
            VERBATIM
        )
    endif()

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/SKSE/Plugins/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
        # Copy the entire archive output directory into OUTPUT_FOLDER
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" "${OUTPUT_FOLDER}"
        # Zip the archive output directory
        COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}.zip" --format=zip "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
        VERBATIM
    )

endif()
