cmake_minimum_required(VERSION 3.22)
project(kochen LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(EMSCRIPTEN)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

option(KOCHEN_BUILD_APP "Build SDL3 + OpenGL interactive Synthesizer Studio" ON)
option(KOCHEN_BUILD_TESTS "Build unit, broad-sample, and E2E tests (native only)" ON)
option(KOCHEN_COVERAGE "Enable gcov coverage flags on core and headless UI/audio targets" OFF)
option(KOCHEN_SANITIZERS "Enable ASan + UBSan on test builds" OFF)

set(KOCHEN_CORE_SOURCES
    src/core/kochen_data.c
    src/core/kochen_physics.c
    src/core/kochen_solver.c
    src/core/kochen_dag.c
    src/core/kochen_project.c
    src/core/kochen_codec.c
)

if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/kochen_studio.c")
    list(APPEND KOCHEN_CORE_SOURCES
        src/ui/kochen_studio.c
        src/ui/kochen_audio.c
    )
    set(KOCHEN_HAS_STUDIO_CORE ON)
else()
    set(KOCHEN_HAS_STUDIO_CORE OFF)
endif()

add_library(kochen_core STATIC ${KOCHEN_CORE_SOURCES})
target_include_directories(kochen_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
if(NOT MSVC)
    target_compile_options(kochen_core PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(kochen_core PUBLIC m)
else()
    target_compile_options(kochen_core PRIVATE /W3 /D_CRT_SECURE_NO_WARNINGS)
endif()

if(KOCHEN_COVERAGE AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(kochen_core PRIVATE --coverage -O0 -g)
    target_link_options(kochen_core PUBLIC --coverage)
endif()
if(KOCHEN_SANITIZERS AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(kochen_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(kochen_core PUBLIC -fsanitize=address,undefined)
endif()

if(KOCHEN_BUILD_APP AND EXISTS "${CMAKE_SOURCE_DIR}/src/ui/main.c")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.30
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    add_executable(kochen
        src/ui/main.c
        src/ui/kochen_gl.c
    )
    target_link_libraries(kochen PRIVATE kochen_core SDL3::SDL3)
    if(NOT MSVC)
        target_compile_options(kochen PRIVATE -Wall -Wextra)
    else()
        target_compile_options(kochen PRIVATE /W3 /D_CRT_SECURE_NO_WARNINGS)
    endif()

    if(EMSCRIPTEN)
        set(KOCHEN_WEB_DIST "${CMAKE_BINARY_DIR}/dist")
        set_target_properties(kochen PROPERTIES
            OUTPUT_NAME "kochen"
            SUFFIX ".js"
            RUNTIME_OUTPUT_DIRECTORY "${KOCHEN_WEB_DIST}"
        )
        target_link_options(kochen PRIVATE
            "-sUSE_WEBGL2=1"
            "-sFULL_ES3=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=33554432"
            "-sFORCE_FILESYSTEM=1"
            "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_kochen_web_load_patch','_kochen_web_get_patch','_kochen_web_get_markdown','_kochen_web_set_touch_hint']"
            "-sEXPORTED_RUNTIME_METHODS=['FS','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
        )
        add_custom_command(TARGET kochen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/web/shell.html" "${KOCHEN_WEB_DIST}/index.html"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${KOCHEN_WEB_DIST}/assets"
            COMMENT "Staging loose external assets/ and index.html beside kochen.wasm"
        )
    else()
        if(APPLE)
            find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
            target_link_libraries(kochen PRIVATE ${OPENGL_FRAMEWORK})
        elseif(WIN32)
            target_link_libraries(kochen PRIVATE opengl32)
        else()
            find_package(OpenGL REQUIRED)
            target_link_libraries(kochen PRIVATE OpenGL::GL)
        endif()

        add_custom_command(TARGET kochen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:kochen>/assets"
            COMMENT "Copying loose external assets/ beside native kochen binary"
        )
    endif()
endif()

if(KOCHEN_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    function(kochen_add_test test_name test_src)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE kochen_core)
        target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR})
        target_compile_definitions(${test_name} PRIVATE KOCHEN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        if(NOT MSVC)
            target_compile_options(${test_name} PRIVATE -Wall -Wextra -Werror)
        endif()
        if(KOCHEN_SANITIZERS AND NOT MSVC)
            target_compile_options(${test_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${test_name} PRIVATE -fsanitize=address,undefined)
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endfunction()

    kochen_add_test(test_data_codec tests/test_data_codec.c)
    kochen_add_test(test_physics_dag tests/test_physics_dag.c)
    kochen_add_test(test_projections_i18n tests/test_projections_i18n.c)
    kochen_add_test(test_broad_sample tests/test_broad_sample.c)
    if(KOCHEN_HAS_STUDIO_CORE AND EXISTS "${CMAKE_SOURCE_DIR}/tests/test_e2e_synth.c")
        kochen_add_test(test_e2e_synth tests/test_e2e_synth.c)
    endif()
endif()
