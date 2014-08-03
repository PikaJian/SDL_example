add_executable(HelloWorld HelloWorld.cpp)
target_link_libraries(HelloWorld ${LIBRARIES})
set_target_properties(HelloWorld PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

