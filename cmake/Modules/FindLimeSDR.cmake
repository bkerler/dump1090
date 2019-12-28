message(STATUS "FINDING LimeSuite.")
find_library(LimeSuite names LimeSuite libLimeSuite
        PATHS ${LimeSuite_PKG_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /usr/lib/arm-linux-gnueabihf
        $ENV{PYBOMBS_PREFIX}/lib
        $ENV{PYBOMBS_PREFIX}/lib64
        )

find_path(LimeSuite_INCLUDE
        NAMES LimeSuite.h
        PATHS
        /usr/include/lime
        /usr/local/include/lime
        $ENV{PYBOMBS_PREFIX}/include
        $ENV{PYBOMBS_PREFIX}/include/lime
        )

if(LimeSuite_INCLUDE AND LimeSuite)
    message(STATUS "Found LimeSuite: ${LimeSuite_INCLUDE}, ${LimeSuite}")
    set(LIB_INCLUDE_DIRS "${LIB_INCLUDE_DIRS} ${LimeSuite_INCLUDE}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${LimeSuite_INCLUDE} -DENABLE_LIMESDR")
    set(SDR_LIBS "${SDR_LIBS} ${LimeSuite}")
    set(SDR_SOURCES ${SDR_SOURCES} sdr_lime.c sdr_lime.h)
endif()