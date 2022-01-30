cmake_minimum_required(VERSION 3.8)
project(Secasy)

set(CMAKE_C_STANDARD 11)
set($ (pkg-config --cflags --libs glib-2.0))

set(SOURCE_FILES main.c Calculations.c InitializationPhase.c Printing.c ProcessingPhase.c SieveOfEratosthenes.c Calculations.h Defines.h InitializationPhase.h Printing.h ProcessingPhase.h SieveOfEratosthenes.h)
add_executable(Secasy ${SOURCE_FILES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

target_link_libraries(Secasy ${GTK3_LIBRARIES})



