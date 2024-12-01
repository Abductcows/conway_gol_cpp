cmake_minimum_required(VERSION 3.30)
project(conway_gol_cpp)

set(CMAKE_CXX_STANDARD 23)

add_executable(conway_gol_cpp src/main.cpp)

### GLUT
add_subdirectory(libs/freeglut)
include_directories(libs/freeglut/include)
target_link_libraries(conway_gol_cpp freeglut)

add_custom_command(TARGET conway_gol_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-debug/libs/freeglut/bin/libfreeglut.dll
        $<TARGET_FILE_DIR:conway_gol_cpp>)
