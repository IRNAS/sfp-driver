cmake_minimum_required(VERSION 2.6)

project(sfp-driver C)
add_definitions(-Os -Wall -Werror --std=gnu99 -Wmissing-declarations -g3)

find_path(ubus_include_dir libubus.h)
include_directories(${ubus_include_dir})

find_path(ubox_include_dir libubox/usock.h)
include_directories(${ubox_include_dir})

find_path(uci_include_dir uci.h)
include_directories(${uci_include_dir})

find_library(ubus_library NAMES ubus)
find_library(ubox_library NAMES ubox)
find_library(uci_library NAMES uci)

set(SOURCES
main.c
sfp.c
ubus.c
)

set(LIBS
${ubox_library}
${ubus_library}
${uci_library}
)

add_executable(sfp-driver ${SOURCES})
target_link_libraries(sfp-driver ${LIBS})

install(TARGETS sfp-driver
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
