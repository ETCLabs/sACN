add_executable(sacn_cpp_source_example
  src/network_select.cpp
  src/sacn_source_example.cpp
  src/main.cpp
)
if(WIN32)
  target_sources(sacn_cpp_source_example PRIVATE src/windows/keyboard_interrupt.cpp)
elseif(UNIX)
  target_sources(sacn_cpp_source_example PRIVATE src/linux/keyboard_interrupt.cpp)
else()
  target_sources(sacn_cpp_source_example PRIVATE src/other/keyboard_interrupt.cpp)
endif()

target_link_libraries(sacn_cpp_source_example PRIVATE sACN)
if(NOT COMPILING_AS_OSS)
  target_clang_tidy(sacn_cpp_source_example)
endif()
set_target_properties(sacn_cpp_source_example PROPERTIES FOLDER examples)
set_target_properties(sacn_cpp_source_example PROPERTIES CXX_STANDARD 14 FOLDER examples)