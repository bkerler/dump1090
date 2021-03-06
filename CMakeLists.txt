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

cmake_minimum_required(VERSION 3.5.1)

project(dump1090)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -std=c11 -Wall -Werror")

set(LIB_SOURCES
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
		)

if(APPLE)
	#for darwin build
	set(COMPAT_SOURCES compat/clock_nanosleep/clock_nanosleep.c compat/clock_nanosleep/clock_nanosleep.h)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_NANOSLEEP -DCLOCKID_T")
elseif(UNIX)
	#for linux build
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE")
	set(COMPAT_SOURCES)
else()
	#for ?? build (probably broken)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	set(COMPAT_SOURCES)
endif()

link_directories(/usr/local/lib)
include_directories(/usr/local/include lib1090/src)

add_subdirectory(lib1090)
add_subdirectory(dump1090)