cmake_minimum_required(VERSION 3.30)
project(conway_gol_cpp)

set(CMAKE_CXX_STANDARD 23)

add_executable(conway_gol_cpp src/main.cpp)

### GLUT
include(FetchContent)
FetchContent_Declare(
        freeglut
        GIT_REPOSITORY https://github.com/freeglut/freeglut.git
        GIT_TAG v3.6.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freeglut)
include_directories(${CMAKE_BINARY_DIR}/_deps/freeglut-src/include)
target_link_libraries(conway_gol_cpp freeglut)

add_custom_command(TARGET conway_gol_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/_deps/freeglut-build/bin/libfreeglut.dll
        $<TARGET_FILE_DIR:conway_gol_cpp>)

add_dependencies(conway_gol_cpp freeglut)