cmake_minimum_required(VERSION 3.1)
project(luaogg)

option(BUILD_SHARED_LIBS "Build modules as shared libraries" ON)
find_package(Ogg REQUIRED)

if(LUA_VERSION)
  find_package(Lua ${LUA_VERSION} EXACT REQUIRED)
else()
  find_package(Lua REQUIRED)
  set(LUA_VERSION "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}" CACHE STRING "Lua version to use")
endif()

set(CMODULE_INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib/lua/${LUA_VERSION}")
set(LUAMODULE_INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/share/lua/${LUA_VERSION}")

set(luaogg_sources)
list(APPEND luaogg_sources "csrc/luaogg.c")

add_library(luaogg ${luaogg_sources})

target_link_libraries(luaogg PRIVATE ${OGG_LIBRARIES})
target_link_directories(luaogg PRIVATE ${OGG_LIBRARY_DIRS})
if(WIN32)
    target_link_libraries(luaogg PRIVATE ${LUA_LIBRARIES})
endif()
target_include_directories(luaogg PRIVATE ${OGG_INCLUDEDIR})
target_include_directories(luaogg PRIVATE ${LUA_INCLUDE_DIR})

if(APPLE)
    set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS} -undefined dynamic_lookup")
    if(BUILD_SHARED_LIBS)
        set_target_properties(luaogg PROPERTIES SUFFIX ".so")
    endif()
endif()

set_target_properties(luaogg PROPERTIES PREFIX "")
set_target_properties(luaogg PROPERTIES OUTPUT_NAME "luaogg")
set_target_properties(luaogg PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(luaogg PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(luaogg PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
  set_target_properties(luaogg PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}")
  set_target_properties(luaogg PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}")
  set_target_properties(luaogg PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}")
endforeach()

install(TARGETS luaogg
  LIBRARY DESTINATION "${CMODULE_INSTALL_LIB_DIR}"
  RUNTIME DESTINATION "${CMODULE_INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${CMODULE_INSTALL_LIB_DIR}"
)
