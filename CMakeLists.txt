
cmake_minimum_required(VERSION 2.8)

project(usbcomm)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add compiler flags for building executables (-fPIE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

aux_source_directory(./source SRC_LIST)

FILE(GLOB_RECURSE INC_LIST "include/*.h" "include/*.hpp")



### Configure Compiler ########################################################
include_directories(
    ./include
)
# link_directories(${Boost_LIBRARY_DIRS})

### Make the executable #######################################################
add_executable(${PROJECT_NAME} ${SRC_LIST} ${INC_LIST})
message("Include list:" ${INC_LIST})
message("Src list:" ${SRC_LIST})
SET(CMAKE_BUILD_TYPE "Release")

target_link_libraries(${PROJECT_NAME} pthread ftd2xx)


### Once it's built... ########################################################
# Here, you would have commands to install your program.
# We will skip these in this example.
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/)
