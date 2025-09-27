cmake_minimum_required(VERSION 3.8)
project(Secasy C)

set(CMAKE_C_STANDARD 11)

# Source files
set(SOURCE_FILES
        main.c
        Calculations.c
        InitializationPhase.c
        Printing.c
        ProcessingPhase.c
        SieveOfEratosthenes.c
        util.c
        Calculations.h
        Defines.h
        InitializationPhase.h
        Printing.h
        ProcessingPhase.h
        SieveOfEratosthenes.h
        util.h)

add_executable(Secasy ${SOURCE_FILES})

# Large file support for Unix-like systems
if (NOT WIN32)
    target_compile_definitions(Secasy PRIVATE _FILE_OFFSET_BITS=64)
endif()

# Warning / optimization flags (do not force -Ofast so user can choose with CMAKE_BUILD_TYPE)
if (MSVC)
    target_compile_options(Secasy PRIVATE /W4 /permissive-)
else()
    target_compile_options(Secasy PRIVATE -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wstrict-prototypes -Wconversion -Wno-unused-parameter)
endif()

# Optional: GTK3 (only if really needed)
option(SECASY_WITH_GTK3 "Build with GTK3 (not required for core hash)" OFF)
if (SECASY_WITH_GTK3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(Secasy PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_directories(Secasy PRIVATE ${GTK3_LIBRARY_DIRS})
    target_compile_definitions(Secasy PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(Secasy PRIVATE ${GTK3_LIBRARIES})
endif()

# Link math library if available (ceil used in ProcessingPhase)
if (NOT WIN32)
    target_link_libraries(Secasy PRIVATE m)
endif()
