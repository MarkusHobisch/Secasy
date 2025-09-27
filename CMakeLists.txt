cmake_minimum_required(VERSION 3.16)
project(Secasy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# Avalanche test executable (exclude main.c, include avalanche.c)
add_executable(SecasyAvalanche
    tests/avalanche/avalanche.c
        Calculations.c
        InitializationPhase.c
        Printing.c
        ProcessingPhase.c
        SieveOfEratosthenes.c
        util.c
        Defines.h
        Calculations.h
        InitializationPhase.h
        Printing.h
        ProcessingPhase.h
        SieveOfEratosthenes.h
        util.h)

# Ensure subdirectory test sources can include headers at project root
target_include_directories(SecasyAvalanche PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Large file support for Unix-like systems
if (NOT WIN32)
    target_compile_definitions(Secasy PRIVATE _FILE_OFFSET_BITS=64)
    target_compile_definitions(SecasyAvalanche PRIVATE _FILE_OFFSET_BITS=64)
endif()

# Warning / optimization flags (do not force -Ofast so user can choose with CMAKE_BUILD_TYPE)
if (MSVC)
    target_compile_options(Secasy PRIVATE /W4 /permissive-)
    target_compile_options(SecasyAvalanche PRIVATE /W4 /permissive-)
else()
    target_compile_options(Secasy PRIVATE -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wstrict-prototypes -Wconversion -Wno-unused-parameter)
    target_compile_options(SecasyAvalanche PRIVATE -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wstrict-prototypes -Wconversion -Wno-unused-parameter)
    # Enable ANSI stdio on MinGW so %zu etc. are recognized and reduce format warnings
    if (MINGW)
        target_compile_definitions(Secasy PRIVATE __USE_MINGW_ANSI_STDIO=1)
        target_compile_definitions(SecasyAvalanche PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
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
    target_link_libraries(SecasyAvalanche PRIVATE m)
endif()
