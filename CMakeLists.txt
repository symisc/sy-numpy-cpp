cmake_minimum_required(VERSION 3.16)

project(synumpy VERSION 1.9.7 LANGUAGES CXX)

include(GNUInstallDirs)

option(SYNUMPY_BUILD_EXAMPLE "Build the example executable" ON)

add_library(synumpy synumpy.cpp)
add_library(syNumpy::synumpy ALIAS synumpy)

target_include_directories(synumpy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(synumpy PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(synumpy PRIVATE /W4 /permissive-)
else()
    target_compile_options(synumpy PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(synumpy PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(SYNUMPY_BUILD_EXAMPLE)
    add_executable(synumpy_example example.cpp)
    target_link_libraries(synumpy_example PRIVATE synumpy)
    set_target_properties(synumpy_example PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()

install(TARGETS synumpy
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/synumpy.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(SYNUMPY_BUILD_EXAMPLE)
    install(TARGETS synumpy_example
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
