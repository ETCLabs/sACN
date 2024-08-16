
add_executable(sacn_receiver_example
  src/main.c
)
if(WIN32)
  target_sources(sacn_receiver_example PRIVATE src/windows/keyboard_interrupt.c)
elseif(UNIX)
  target_sources(sacn_receiver_example PRIVATE src/linux/keyboard_interrupt.c)
else()
  target_sources(sacn_receiver_example PRIVATE src/other/keyboard_interrupt.c)
endif()

target_link_libraries(sacn_receiver_example PRIVATE sACN)
if(NOT COMPILING_AS_OSS)
  target_clang_tidy(sacn_receiver_example)
endif()
set_target_properties(sacn_receiver_example PROPERTIES FOLDER examples)
