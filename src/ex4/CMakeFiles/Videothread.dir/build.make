# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/pikachu123/SDL_example

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/pikachu123/SDL_example

# Include any dependencies generated for this target.
include src/ex4/CMakeFiles/Videothread.dir/depend.make

# Include the progress variables for this target.
include src/ex4/CMakeFiles/Videothread.dir/progress.make

# Include the compile flags for this target's objects.
include src/ex4/CMakeFiles/Videothread.dir/flags.make

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o: src/ex4/CMakeFiles/Videothread.dir/flags.make
src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o: src/ex4/video_thread.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/pikachu123/SDL_example/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o"
	cd /home/pikachu123/SDL_example/src/ex4 && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/Videothread.dir/video_thread.cpp.o -c /home/pikachu123/SDL_example/src/ex4/video_thread.cpp

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Videothread.dir/video_thread.cpp.i"
	cd /home/pikachu123/SDL_example/src/ex4 && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/pikachu123/SDL_example/src/ex4/video_thread.cpp > CMakeFiles/Videothread.dir/video_thread.cpp.i

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Videothread.dir/video_thread.cpp.s"
	cd /home/pikachu123/SDL_example/src/ex4 && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/pikachu123/SDL_example/src/ex4/video_thread.cpp -o CMakeFiles/Videothread.dir/video_thread.cpp.s

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.requires:
.PHONY : src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.requires

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.provides: src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.requires
	$(MAKE) -f src/ex4/CMakeFiles/Videothread.dir/build.make src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.provides.build
.PHONY : src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.provides

src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.provides.build: src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o

# Object files for target Videothread
Videothread_OBJECTS = \
"CMakeFiles/Videothread.dir/video_thread.cpp.o"

# External object files for target Videothread
Videothread_EXTERNAL_OBJECTS =

bin/Videothread: src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o
bin/Videothread: src/ex4/CMakeFiles/Videothread.dir/build.make
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libSDL2.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libavcodec.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libavformat.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libswscale.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libavutil.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libavdevice.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libavfilter.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libswresample.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libx264.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libvpx.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libopus.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libmp3lame.so
bin/Videothread: /usr/lib64/libogg.so
bin/Videothread: /usr/lib64/libvorbisfile.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libfdk-aac.so
bin/Videothread: /home/pikachu123/ffmpeg_build/lib/libpostproc.so
bin/Videothread: /usr/lib64/libvorbisenc.so
bin/Videothread: /usr/lib64/libvorbis.so
bin/Videothread: src/ex4/CMakeFiles/Videothread.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable ../../bin/Videothread"
	cd /home/pikachu123/SDL_example/src/ex4 && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Videothread.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/ex4/CMakeFiles/Videothread.dir/build: bin/Videothread
.PHONY : src/ex4/CMakeFiles/Videothread.dir/build

src/ex4/CMakeFiles/Videothread.dir/requires: src/ex4/CMakeFiles/Videothread.dir/video_thread.cpp.o.requires
.PHONY : src/ex4/CMakeFiles/Videothread.dir/requires

src/ex4/CMakeFiles/Videothread.dir/clean:
	cd /home/pikachu123/SDL_example/src/ex4 && $(CMAKE_COMMAND) -P CMakeFiles/Videothread.dir/cmake_clean.cmake
.PHONY : src/ex4/CMakeFiles/Videothread.dir/clean

src/ex4/CMakeFiles/Videothread.dir/depend:
	cd /home/pikachu123/SDL_example && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/pikachu123/SDL_example /home/pikachu123/SDL_example/src/ex4 /home/pikachu123/SDL_example /home/pikachu123/SDL_example/src/ex4 /home/pikachu123/SDL_example/src/ex4/CMakeFiles/Videothread.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/ex4/CMakeFiles/Videothread.dir/depend

