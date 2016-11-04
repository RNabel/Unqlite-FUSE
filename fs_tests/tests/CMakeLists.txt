include_directories(${gtest_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/fs)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -D_FILE_OFFSET_BITS=64 -fpermissive -luuid")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -D_FILE_OFFSET_BITS=64 -luuid -std=c11 ")

include_directories(/usr/include/fuse)
set(CURRENT_DIR .)


add_executable(runBasicFsTests fs_tests.cpp
        ${PROJECT_SOURCE_DIR}/fs/newfs.c)
target_link_libraries(runBasicFsTests uuid fuse pthread)

# Link test libraries.
target_link_libraries(runBasicFsTests gtest gtest_main fuse pthread )

target_link_libraries(runBasicFsTests newfs_lib)