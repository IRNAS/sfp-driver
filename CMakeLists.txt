cmake_minimum_required(VERSION 2.6)

project(sfp-driver C)
add_definitions(-Os -Wall -Werror --std=gnu99 -Wmissing-declarations -g3)

set(SOURCES
main.c
sfp.c
ubus.c
)

set(LIBS
  ubox
  ubus
  uci
)

add_executable(sfp-driver ${SOURCES})
target_link_libraries(sfp-driver ${LIBS})

install(TARGETS sfp-driver
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
