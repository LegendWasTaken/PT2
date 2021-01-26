cmake_minimum_required(VERSION 3.16)
project(PT2)

set(CMAKE_CXX_STANDARD 17)
find_package(embree 3.0 REQUIRED)
include(FetchContent)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG 3.3.2
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 0.9.9.8
)

FetchContent_MakeAvailable(glfw glm)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG v1.76
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)

    add_library(lib_imgui ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
    target_include_directories(lib_imgui PUBLIC ${imgui_SOURCE_DIR})

endif ()

add_executable(PT2
        src/main.cpp
        src/pt2/pt2.cpp
        src/pt2/structs.h
        src/imgui/imgui_impl_glfw.cpp
        src/imgui/imgui_impl_opengl3.cpp
        src/glad/glad.c
        src/pt2/thread_pool.cpp
        )

target_include_directories(PT2 PUBLIC "extern")
target_include_directories(PT2 PUBLIC "src")
target_link_libraries(PT2 glfw embree glm lib_imgui)
#target_compile_options(PT2 PUBLIC -fsanitize=thread)
#target_link_options(PT2 PUBLIC -fsanitize=thread)
