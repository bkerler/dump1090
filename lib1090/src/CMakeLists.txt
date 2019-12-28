# dump1090 - ADS-B software radio decoder
#
# Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

message(${CMAKE_MODULE_PATH})

find_library(usb-1.0 NAMES libusb-1.0 libusb usb-1.0)
find_path(libusb_INCLUDE libusb.h
        HINTS
        /usr/include/libusb-1.0
        /usr/local/include/libusb-1.0
        /usr/include/libusb
        /usr/local/include/libusb
        )

set(LIB_INCLUDE_DIRS ${libusb_INCLUDE})
set(LIB_INCLUDE_DIRS "/usr/local/lib ${LIB_INCLUDE_DIRS}")

set(SDR_SOURCES sdr.c sdr.h sdr_ifile.c sdr_ifile.h)
set(SDR_LIBS)

find_package(RtlSDR REQUIRED)
find_package(SoapySDR)
find_package(LimeSDR)
find_package(BladeRF)


add_library(1090 SHARED
        ais_charset.c ais_charset.h
        anet.c anet.h
        comm_b.c comm_b.h
        convert.c convert.h
        cpr.c cpr.h
        crc.c crc.h
        demod_2400.c demod_2400.h
        icao_filter.c icao_filter.h
        interactive.c
        lib1090.c lib1090.h
        mode_ac.c
        mode_s.c mode_s.h
        net_io.c net_io.h
        stats.c stats.h
        track.c track.h
        util.c util.h
        dump1090.c
        faup1090.c
        view1090.c
        ${COMPAT_SOURCES}
        ${SDR_SOURCES}
        )

target_link_libraries(1090
        ${SDR_LIBS}
        rtlsdr
        m
        ncurses
        usb-1.0
        pthread
        )

target_include_directories(1090 PRIVATE ${LIB_INCLUDE_DIRS})

set_target_properties(1090 PROPERTIES
        VERSION 0.4.0
        SOVERSION 0
        )

INSTALL(
        TARGETS 1090
        LIBRARY DESTINATION lib
        #FILES lib1090.h
        #DESTINATION include
)