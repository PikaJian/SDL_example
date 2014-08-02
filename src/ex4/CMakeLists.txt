add_executable(Videothread video_thread.cpp)
target_link_libraries(Videothread ${LIBRARIES})
set_target_properties(Videothread PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

