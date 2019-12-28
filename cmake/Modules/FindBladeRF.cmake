message(STATUS "FINDING BLADERF.")

find_library(bladeRF_LIBRARY NAMES libbladeRF bladeRF
        PATHS ${LimeSuite_PKG_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /usr/lib/arm-linux-gnueabihf
        $ENV{PYBOMBS_PREFIX}/lib
        $ENV{PYBOMBS_PREFIX}/lib64
        )

find_path(bladeRF_INCLUDE_DIR NAMES libbladeRF.h
        PATHS ${BLADERF_PKG_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        $ENV{PYBOMBS_PREFIX}/include
        )

if (bladeRF_LIBRARIES AND bladeRF_INCLUDE_DIR)
    message(STATUS "Found libbladeRF: ${bladeRF_INCLUDE_DIR}, ${bladeRF_LIBRARY}")
    set(LIB_INCLUDE_DIRS "${LIB_INCLUDE_DIRS} ${bladeRF_INCLUDE}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${bladeRF_INCLUDE} -DENABLE_BLADERF")
    set(SDR_LIBS "${SDR_LIBS} ${bladeRF}")
    set(SDR_SOURCES ${SDR_SOURCES} sdr_bladerf.c sdr_bladerf.h)
endif()


