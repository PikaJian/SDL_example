cmake_minimum_required(VERSION 2.8)
project(test)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${test_SOURCE_DIR}/cmake")
<<<<<<< HEAD
set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ -std=c++11 -Wall -pg -p")
SET(CMAKE_EXE_LINKER_FLAGS  "-lc++ -lc++abi" )
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   message("Using Clang")
     # using Clang
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       # using GCC
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
         # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
           # using Visual Studio C++
endif()
#set(CMAKE_CXX_FLAGS "-std=c++0x -Wall -pg -g -ggdb")
=======
set(CMAKE_CXX_FLAGS "-std=c++0x -Wall -pg -g -ggdb")
>>>>>>> 02daf5209c9a9b70d5c80abb00524633ce4f2082
find_package(FFMPEG_BUILD REQUIRED)
if (NOT FFMPEG_BUILD_FOUND)
   message(FATAL_ERROR "Libary FFMPEG_BUILD not found")
endif (NOT FFMPEG_BUILD_FOUND)
add_definitions(${FFMPEG_BUILD_DEFINITIONS})
include_directories(${FFMPEG_BUILD_INCLUDE_DIR})
set(LIBRARIES ${FFMPEG_BUILD_LIBRARIES} )
#message(${FFMPEG_BUILD_INCLUDE_DIR})
message(${LIBRARIES})
<<<<<<< HEAD
#add_subdirectory(src/ex1)
#add_subdirectory(src/ex4)
add_subdirectory(src/ex5)
#add_subdirectory(src/gtk)
=======
add_subdirectory(src/ex1)
add_subdirectory(src/ex4)
add_subdirectory(src/ex5)
add_subdirectory(src/gtk)
>>>>>>> 02daf5209c9a9b70d5c80abb00524633ce4f2082
