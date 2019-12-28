message(STATUS "FINDING SoapySDR.")
find_path(SOAPYSDR_INCLUDE_DIRS
        NAMES Device.h
        PATHS ${SOAPYSDR_PKG_INCLUDE_DIRS}
        /usr/include/SoapySDR
        /usr/local/include/SoapySDR
        $ENV{PYBOMBS_PREFIX}/include/SoapySDR
        )

find_library(SOAPYSDR_LIBRARIES
        NAMES libSoapySDR SoapySDR
        PATHS ${SOAPYSDR_PKG_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /usr/lib/arm-linux-gnueabihf
        $ENV{PYBOMBS_PREFIX}/lib
        $ENV{PYBOMBS_PREFIX}/lib64
        )

if(SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LIBRARIES)
    set(SOAPYSDR_FOUND TRUE CACHE INTERNAL "libSOAPYSDR found")
    message(STATUS "Found libSOAPYSDR: ${SOAPYSDR_INCLUDE_DIRS}, ${SOAPYSDR_LIBRARIES}")
    set(LIB_INCLUDE_DIRS "${LIB_INCLUDE_DIRS} ${SOAPYSDR_LIBRARIES} ${SOAPYSDR_INCLUDE_DIRS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${SOAPYSDR_INCLUDE_DIRS} -DENABLE_SOAPYSDR")
    set(SDR_LIBS "${SDR_LIBS} ${SOAPYSDR_LIBRARIES}")
    set(SDR_SOURCES ${SDR_SOURCES} sdr_soapysdr.h sdr_soapysdr.c)
endif(SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LIBRARIES)