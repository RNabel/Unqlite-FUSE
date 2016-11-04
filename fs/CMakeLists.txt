cmake_minimum_required(VERSION 3.0)

# Replaces -I flags.
include_directories(/usr/include/fuse /usr/include/)

# Replaces LIBS.
set(CURRENT_DIR .)

# Sets C flags in CFLAGS and LIBS
set(LIBS  "-luuid -lfuse -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -D_FILE_OFFSET_BITS=64 -luuid")

# Sets dependencies.
set(DEPS fs.c unqlite.c)

# Create all targets.
set(TARGET1 "store")
set(TARGET2 "fetch")
set(TARGET3 "newfs")
set(TARGET4 "testProg")
set(TARGET5 "uuid1")

set(SOURCE_TAR1 "${TARGET1}.c")
set(SOURCE_TAR2 "${TARGET2}.c" newfs.h)
set(SOURCE_TAR3 "${TARGET3}.c" newfs.h)
set(SOURCE_TAR4 "${TARGET4}.c")
set(SOURCE_TAR5 "${TARGET5}.c")

add_executable(${TARGET1} ${SOURCE_TAR1} ${DEPS})
target_link_libraries(${TARGET1} uuid fuse pthread)

add_executable(${TARGET2} ${SOURCE_TAR2} ${DEPS})
target_link_libraries(${TARGET2} uuid fuse pthread)

# newfs
add_executable(${TARGET3} ${SOURCE_TAR3} fs.c unqlite.c)
SET_TARGET_PROPERTIES(${TARGET3} PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${TARGET3} uuid fuse pthread)

# testProg
#add_executable(${TARGET4} ${SOURCE_TAR4})
#target_link_libraries(${TARGET4} uuid fuse pthread)

add_library(test_lib newfs.c fs.c unqlite.c)
SET_TARGET_PROPERTIES(test_lib PROPERTIES
        COMPILE_FLAGS "-DIS_LIB ${SHARED_FLAGS}"
        )

target_link_libraries(test_lib uuid fuse pthread)


add_executable(check_test check_tests.c)
SET_TARGET_PROPERTIES(check_test PROPERTIES
        COMPILE_FLAGS "-DIS_LIB ${SHARED_FLAGS}"
        )
target_link_libraries(check_test test_lib check m rt)

#add_executable(${TARGET5} uuid.c)
#target_link_libraries(check_test uuid )


# Create library which can be tested with Google Test.
#add_library(newfs_lib SHARED ${SOURCE_TAR3} ${DEPS} newfs.h)
#target_link_libraries(newfs_lib fuse pthread)
