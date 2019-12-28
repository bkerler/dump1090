find_path(rtlsdr_INCLUDE rtl-sdr.h
        PATHS ${RTLSDR_PKG_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        $ENV{PYBOMBS_PREFIX}/include
        )

find_library(rtlsdr NAMES librtlsdr rtlsdr
        PATHS ${RTLSDR_PKG_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /usr/lib/arm-linux-gnueabihf
        $ENV{PYBOMBS_PREFIX}/lib
        $ENV{PYBOMBS_PREFIX}/lib64
        )

if (True)
    message(STATUS "Found librtlsdr: ${rtlsdr_INCLUDE}, ${rtlsdr}")
    set(LIB_INCLUDE_DIRS "${LIB_INCLUDE_DIRS} ${rtlsdr_INCLUDE}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${rtlsdr_INCLUDE} -DENABLE_RTLSDR")
    set(SDR_LIBS rtlsdr)
    set(SDR_SOURCES ${SDR_SOURCES} sdr_rtlsdr.c sdr_rtlsdr.h)
endif()