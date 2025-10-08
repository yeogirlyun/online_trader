# Quote Simulation Module CMakeLists.txt
# ======================================

cmake_minimum_required(VERSION 3.16)
project(quote_simulation)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Eigen3 REQUIRED)
find_package(nlohmann_json QUIET)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Add MarS Python integration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MarS")
    set(MARS_AVAILABLE TRUE)
    message(STATUS "✅ MarS package found")
else()
    set(MARS_AVAILABLE FALSE)
    message(STATUS "⚠️ MarS package not found - basic simulation only")
endif()

# Source files
set(QUOTE_SIMULATION_SOURCES
    src/online_quote_simulator.cpp
    src/virtual_market.cpp
    src/mars_data_loader.cpp
)

# Header files
set(QUOTE_SIMULATION_HEADERS
    include/online_quote_simulator.hpp
    include/virtual_market.hpp
    include/mars_data_loader.hpp
)

# Create quote simulation library
add_library(quote_simulation STATIC ${QUOTE_SIMULATION_SOURCES} ${QUOTE_SIMULATION_HEADERS})

# Link libraries
target_link_libraries(quote_simulation
    sentio_common
    sentio_backend
    sentio_strategy
    ${EIGEN3_LIBRARIES}
)

# Link nlohmann_json if available
if(nlohmann_json_FOUND)
    target_link_libraries(quote_simulation nlohmann_json::nlohmann_json)
    target_compile_definitions(quote_simulation PRIVATE JSON_FOUND)
endif()

# Compile definitions
target_compile_definitions(quote_simulation PRIVATE
    QUOTE_SIMULATION_VERSION="1.0.0"
    MARS_AVAILABLE=${MARS_AVAILABLE}
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(quote_simulation PRIVATE
        -Wall -Wextra -Wpedantic
        -O3 -march=native
        -fopenmp
    )
    target_link_options(quote_simulation PRIVATE -fopenmp)
endif()

# Create example executable
add_executable(quote_simulation_example examples/quote_simulation_example.cpp)
target_link_libraries(quote_simulation_example quote_simulation)

# Create test executable
# Commented out - file does not exist
# add_executable(quote_simulation_test examples/quote_simulation_test.cpp)
# target_link_libraries(quote_simulation_test quote_simulation)

# Install targets
install(TARGETS quote_simulation
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${QUOTE_SIMULATION_HEADERS}
    DESTINATION include/quote_simulation
)

# Python tools installation
install(PROGRAMS tools/online_quote_simulator.py
    DESTINATION bin
)

install(PROGRAMS tools/mars_bridge.py
    DESTINATION bin
)

# Copy MarS package if available
if(MARS_AVAILABLE)
    install(DIRECTORY MarS/
        DESTINATION share/quote_simulation/MarS
        FILES_MATCHING PATTERN "*.py"
        PATTERN "*.md"
        PATTERN "*.txt"
        PATTERN "*.yaml"
        PATTERN "*.yml"
        PATTERN "*.json"
        PATTERN "*.sh"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Quote Simulation Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  MarS Integration: ${MARS_AVAILABLE}")
message(STATUS "  Eigen3: ${EIGEN3_FOUND}")
message(STATUS "  nlohmann_json: ${nlohmann_json_FOUND}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
