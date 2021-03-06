cmake_minimum_required(VERSION 3.5)

#################################################
#                                               #
#                  Set by user                  #
#                                               #
#################################################


# Location of the DynamoRIO and drrun
# Note, if 64 DynamoRIO is given, only 64 bit applications can be run.
# Likewise, if 32 DynamoRIO is given, only 32 bit applications can be run.
# These will be ignored if DynamoRIO_DIR / DRRUN_PATH are set via the command line
set(DynamoRIO_DIR_default /home/vagrant/dynamorio/build/cmake)
set(DRRUN_PATH_default /home/vagrant/dynamorio/build/bin64/drrun)


### Developer options below ###


# Debug mode options. Comment out to disable
# set(DEBUG_MODE "VERBOSE")
# set(DEBUG_MODE "ON")

# DynamoRIO debug log level (ignored if debug mode off)
# If this is not set, then DR debug is not used
# Note: this requires DynamoRIO to have been build with debuging options
#set(DR_DEBUG_LOG_LEVEL 4)

# The name of the log file (only used for DEBUG_MODE)
# set(DR_SS_LOG_FILE "dr_shadow_stack.log")

# For YouCompleteMe vim plugin
# Disable for non-developers
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


#################################################
#                                               #
#               Program Constants               #
#                                               #
#################################################


# Set the program name
set(PROGRAM_NAME DrShadowStack)
project(${PROGRAM_NAME} C CXX)

# Default mode: int for internal, ext for external
set(DEFAULT_MODE int)


# Set the program version
file(READ ../VERSION PROGRAM_VERSION)
string(STRIP "${PROGRAM_VERSION}" PROGRAM_VERSION)

# The name of the client shared library for DynamoRIO
set(SS_DR_CLIENT_SO ss_dr_client)

# The name of the support library
set(SS_SUPPORT_LIB ss_support)

# Choose the proper DynamoRIO dir and drrun path
if(NOT (DEFINED DynamoRIO_DIR))
    set(DynamoRIO_DIR ${DynamoRIO_DIR_default})
endif()
if(NOT (DEFINED DRRUN_PATH))
    set(DRRUN_PATH ${DRRUN_PATH_default})
endif()


#################################################
#                                               #
#              Automated - General              #
#                                               #
#################################################



# Find packages
find_package(DynamoRIO REQUIRED)
find_package(Boost 1.47.0 COMPONENTS program_options filesystem REQUIRED)

# Require C++ 11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required complication flags
set (CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -Werror" )

# Add macro definitions
add_definitions(-DDRRUN_PATH="${DRRUN_PATH}")
add_definitions(-DVERSION="${PROGRAM_VERSION}")
add_definitions(-DDEFAULT_MODE="${DEFAULT_MODE}")
add_definitions(-DPROGRAM_NAME="${PROGRAM_NAME}")
add_definitions(-DDYNAMORIO_CLIENT_SO="${CMAKE_BINARY_DIR}/lib${SS_DR_CLIENT_SO}.so")


# If debug mode
if(DEFINED DEBUG_MODE)
    message("Debug Mode ON!")

    # If verbose
    if(DEBUG_MODE STREQUAL "VERBOSE")
        message("\t Verbose Debug Mode ON!")
        add_definitions(-DVERBOSE)
    endif()

    # If DynamoRIO debugging requested
    if(DEFINED DR_DEBUG_LOG_LEVEL)
        message("\t DynamoRIO debugging level set to " ${DR_DEBUG_LOG_LEVEL})
        add_definitions(-DDR_DEBUG_LOG_LEVEL="${DR_DEBUG_LOG_LEVEL}")
    endif()

    # Compile the program for debugging
    set(CMAKE_BUILD_TYPE debug)
    add_definitions(-DDEBUG_MODE)
    add_definitions(-DLOG_FILE="${DR_SS_LOG_FILE}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -ggdb -Wno-unused-parameter")

# Not debug mode
else()
    message("Debug Mode OFF!")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()


#################################################
#                                               #
#            Creating the support lib           #
#                                               #
#################################################


# Create the .so
add_library(${SS_SUPPORT_LIB} SHARED
    quick_socket.cpp
    utilities.cpp
    ss_mode.cpp
    message.cpp
    group.cpp
    )


#################################################
#                                               #
#           Creating the client .so's           #
#                                               #
#################################################


# Create the .so
add_library(${SS_DR_CLIENT_SO} SHARED
    dr_shadow_stack_client.cpp
    dr_internal_ss_events.cpp
    dr_external_ss_events.cpp
    dr_print_sym.cpp
    )

# Configure DynamoRIO
configure_DynamoRIO_client(${SS_DR_CLIENT_SO})
use_DynamoRIO_extension(${SS_DR_CLIENT_SO} "drmgr")
use_DynamoRIO_extension(${SS_DR_CLIENT_SO} "drsyms")

# Link to the support library
target_link_libraries(${SS_DR_CLIENT_SO} ${SS_SUPPORT_LIB})


#################################################
#                                               #
#             Creating the project              #
#                                               #
#################################################


# Build the shadow stack executable
add_executable(${PROGRAM_NAME}
    external_stack_server.cpp
    shadow_stack.cpp
    parse_args.cpp
    temp_name.cpp
    )

# Link to the support library
target_link_libraries(${PROGRAM_NAME} ${SS_SUPPORT_LIB} Boost::program_options Boost::filesystem)
