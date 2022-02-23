cmake_minimum_required(VERSION 3.10)

project(VulkanEngine VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/)
include_directories(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/vma/)
include_directories(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/stb/)
include_directories(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/litelogger/)
add_subdirectory(lib/glfw-3.3.6)
add_subdirectory(lib/glm-0.9.9.8)
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME}
    src/main/main.cpp

    src/main/Application.cpp
)

target_link_libraries(${PROJECT_NAME}
        Vulkan::Vulkan
        glfw
        glm
)

file(COPY res DESTINATION ${CMAKE_BINARY_DIR})