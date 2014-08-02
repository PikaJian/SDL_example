# Try to find SDL library
#  Once done this will define
#  FFMPEG_BUILD_FOUND - if system found FFMPEG_BUILD library
#  FFMPEG_BUILD_INCLUDE_DIRS - The FFMPEG_BUILD include directories
#  FFMPEG_BUILD_LIBRARIES - The libraries needed to use FFMPEG_BUILD
#  FFMPEG_BUILD_DEFINITIONS - Compiler switches required for using FFMPEG_#  BUILD

# Uncomment the following line to print which directory CMake is looking in.
#MESSAGE(STATUS "PART4_ROOT_DIR: " ${PART4_ROOT_DIR})
find_path(FFMPEG_BUILD_INCLUDE_DIR
    NAMES x264.h
    PATH_SUFFIXES include
    PATHS $ENV{HOME}/ffmpeg_build
    DOC "The FFMPEG_BUILD include directory"
)
set(needlibs ${needlibs} SDL2 avcodec avformat swscale avutil avdevice avfilter swresample x264 vpx opus mp3lame ogg vorbisfile fdk-aac postproc vorbisenc vorbis)
foreach(name ${needlibs})
   message("Find ${name} in ffmpeg_build")
   find_library(
      LIB
      NAMES ${name}
      PATH_SUFFIXES lib
      PATHS $ENV{HOME}/ffmpeg_build
      DOC "The FFMPEG_BUILD library"
   )
   if(LIB)
      message("Libary ${LIB} Found")
      list(APPEND FFMPEG_BUILD_LIBRARIES ${LIB}) 
   endif(LIB)
   unset(LIB CACHE)
endforeach(name)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOGGING_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(FFMPEG_BUILD DEFAULT_MSG FFMPEG_BUILD_INCLUDE_DIR)

if (FFMPEG_BUILD_FOUND)
   #set(FFMPEG_BUILD_LIBRARIES ${FFMPEG_BUILD_LIBRARY} )
   set(FFMPEG_BUILD_INCLUDE_DIRS ${FFMPEG_BUILD_INCLUDE_DIR} )
   set(FFMPEG_BUILD_DEFINITIONS )
endif()

# Tell cmake GUIs to ignore the "local" variables.
mark_as_advanced(FFMPEG_BUILD_ROOT_DIR FFMPEG_BUILD_INCLUDE_DIR FFMPEG_BUILD_LIBRARIES)
