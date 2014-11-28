# This is an input file for the CMake makefile generator

#
# DDS 2.7.0   A bridge double dummy solver.
# Copyright (C) 2006-2014 by Bo Haglund
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.  See the License for the specific language governing
# permissions and limitations under the License.
#

#
# If you want to run cmake in an interactive mode that gives you prompts
# for build options, use "cmake -i" (or one of the graphical interfaces)
# Otherwise you will get the default build.
#
#       http://en.wikipedia.org/wiki/CMake
#       http://gearbox.sourceforge.net/gbx_doc_buildsys.html
#       http://www.ddj.com/cpp/184405251
#       http://www.elpauer.org/stuff/learning_cmake.pdf
#
# If you just want to get a debug build (e.g. running from a CMake
# project in QtCreator) then provide the command line option:
#
#       -DCMAKE_BUILD_TYPE=Debug
#

project(dds)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(src SRC_LIST)
aux_source_directory(. APP_LIST)

# Note caveat here about how "globbing" doesn't get the right dependencies
# http://stackoverflow.com/questions/1167154/#comment10556582_5843681
file(GLOB INC_LIST include/dds/*.h)

# for gcc >= 4.7, or c++0x for 4.6
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11") 

# http://stackoverflow.com/a/9862800/211160
# temporarily disabling -Wshadow because of winRanks shadowing declaration
# temporarily disabling -Wsign-conversion because of unsigned char in interface
#   (for cardRank, cardSuit, and cardHand in ddds.h)
# temporarily adding -Wno-unknown-pragmas due to openMP
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused -Wno-unknown-pragmas")

add_executable(${PROJECT_NAME} ${SRC_LIST} ${INC_LIST} ${APP_LIST})
include_directories (../dds/include)

MESSAGE( STATUS "SRC_LIST " ${SRC_LIST} )
