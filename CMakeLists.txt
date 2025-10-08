cmake_minimum_required(VERSION 3.16)
project(online_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Performance optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling performance optimizations for Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -funroll-loops -DNDEBUG")
    add_compile_definitions(NDEBUG)
    
    # Enable OpenMP for parallel processing if available
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found - enabling parallel processing")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fopenmp")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find Eigen3 for online learning (REQUIRED for this project)
find_package(Eigen3 3.3 REQUIRED)
message(STATUS "Eigen3 found - Online learning support enabled")
message(STATUS "Eigen3 version: ${EIGEN3_VERSION}")
message(STATUS "Eigen3 include: ${EIGEN3_INCLUDE_DIR}")

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json found - enabling robust JSON parsing")
    add_compile_definitions(NLOHMANN_JSON_AVAILABLE)
else()
    message(STATUS "nlohmann/json not found - using header-only fallback")
endif()

# =============================================================================
# Common Library
# =============================================================================
add_library(online_common
    src/common/types.cpp
    src/common/utils.cpp
    src/common/json_utils.cpp
    src/common/trade_event.cpp
    src/common/binary_data.cpp
    src/common/time_utils.cpp
    src/common/eod_state.cpp
    src/common/nyse_calendar.cpp
    src/core/data_io.cpp
    src/core/data_manager.cpp
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_common PRIVATE nlohmann_json::nlohmann_json)
endif()

# =============================================================================
# Strategy Library (Base Framework for Online Learning)
# =============================================================================
set(STRATEGY_SOURCES
    src/strategy/istrategy.cpp
    src/strategy/ml_strategy_base.cpp
    src/strategy/online_strategy_base.cpp
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/trading_state.cpp
    src/strategy/online_ensemble_strategy.cpp
)

# Add unified feature engine for online learning
list(APPEND STRATEGY_SOURCES src/features/unified_feature_engine.cpp)

add_library(online_strategy ${STRATEGY_SOURCES})
target_link_libraries(online_strategy PRIVATE online_common)
target_link_libraries(online_strategy PUBLIC Eigen3::Eigen)
target_include_directories(online_strategy PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_strategy PRIVATE nlohmann_json::nlohmann_json)
endif()

# Link OpenMP if available for performance optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND OpenMP_CXX_FOUND)
    target_link_libraries(online_strategy PRIVATE OpenMP::OpenMP_CXX)
endif()

# =============================================================================
# Backend Library (Ensemble PSM for Online Learning)
# =============================================================================
add_library(online_backend
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
    src/backend/audit_component.cpp
    src/backend/leverage_manager.cpp
    src/backend/adaptive_portfolio_manager.cpp
    src/backend/adaptive_trading_mechanism.cpp
    src/backend/position_state_machine.cpp
    # Enhanced Dynamic PSM components
    src/backend/dynamic_hysteresis_manager.cpp
    src/backend/dynamic_allocation_manager.cpp
    src/backend/enhanced_position_state_machine.cpp
    src/backend/enhanced_backend_component.cpp
    # Ensemble PSM for online learning (KEY COMPONENT)
    src/backend/ensemble_position_state_machine.cpp
)
target_link_libraries(online_backend PRIVATE online_common)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_backend PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_backend PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Online Learning Library (Core Focus of This Project)
# =============================================================================
add_library(online_learning
    src/learning/online_predictor.cpp
)
target_link_libraries(online_learning PUBLIC 
    online_common 
    online_strategy
    Eigen3::Eigen
)
target_include_directories(online_learning PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)
message(STATUS "Created online_learning library with Eigen3 support")

# =============================================================================
# Testing Framework
# =============================================================================
add_library(online_testing_framework STATIC
    # Core Testing Framework
    src/testing/test_framework.cpp
    src/testing/test_result.cpp
    src/testing/enhanced_test_framework.cpp

    # Validation
    src/validation/strategy_validator.cpp
    src/validation/validation_result.cpp
    src/validation/walk_forward_validator.cpp
    src/validation/bar_id_validator.cpp

    # Analysis
    src/analysis/performance_metrics.cpp
    src/analysis/performance_analyzer.cpp
    src/analysis/temp_file_manager.cpp
    src/analysis/statistical_tests.cpp
    src/analysis/enhanced_performance_analyzer.cpp
)

target_include_directories(online_testing_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(online_testing_framework
    PUBLIC
        online_strategy      # Strategy implementation library
        online_backend       # Backend components
    PRIVATE
        online_common        # Common utilities (only needed internally)
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_testing_framework PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_testing_framework PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Live Trading Library (Alpaca + Polygon WebSocket Integration)
# =============================================================================
find_package(CURL REQUIRED)

# Find libwebsockets for real-time Polygon data
find_library(WEBSOCKETS_LIB websockets HINTS /opt/homebrew/lib)
if(WEBSOCKETS_LIB)
    message(STATUS "libwebsockets found: ${WEBSOCKETS_LIB}")
else()
    message(FATAL_ERROR "libwebsockets not found - install with: brew install libwebsockets")
endif()

add_library(online_live
    src/live/alpaca_client.cpp
    src/live/polygon_websocket.cpp
    src/live/position_book.cpp
)
target_link_libraries(online_live PRIVATE
    online_common
    CURL::libcurl
    ${WEBSOCKETS_LIB}
)
target_include_directories(online_live PRIVATE /opt/homebrew/include)
if(nlohmann_json_FOUND)
    target_link_libraries(online_live PRIVATE nlohmann_json::nlohmann_json)
endif()
message(STATUS "Created online_live library for live trading (Alpaca + Polygon WebSocket)")

# =============================================================================
# CLI Executable (sentio_cli for online learning)
# =============================================================================
add_executable(sentio_cli
    src/cli/sentio_cli_main.cpp
    src/cli/command_interface.cpp
    src/cli/command_registry.cpp
    src/cli/parameter_validator.cpp
    # Online learning commands (commented out - missing XGBFeatureSet implementations)
    # src/cli/online_command.cpp
    # src/cli/online_sanity_check_command.cpp
    # src/cli/online_trade_command.cpp
    # OnlineEnsemble workflow commands
    src/cli/generate_signals_command.cpp
    src/cli/execute_trades_command.cpp
    src/cli/analyze_trades_command.cpp
    # Live trading command
    src/cli/live_trade_command.cpp
)

# Link all required libraries
# Note: online_strategy, online_backend, and online_common are transitively included
# via online_learning and online_testing_framework, so we don't list them explicitly
target_link_libraries(sentio_cli PRIVATE
    online_learning          # brings in online_strategy + online_common
    online_testing_framework # brings in online_strategy + online_backend + online_common
    online_live             # brings in online_common
)

# Add nlohmann/json include for CLI
if(nlohmann_json_FOUND)
    target_link_libraries(sentio_cli PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(sentio_cli PRIVATE /opt/homebrew/include)
endif()

message(STATUS "Created sentio_cli executable with online learning support")

# Create standalone test executable for online learning
add_executable(test_online_trade tools/test_online_trade.cpp)
target_link_libraries(test_online_trade PRIVATE 
    online_learning
    online_strategy
    online_backend
    online_common
)
message(STATUS "Created test_online_trade executable")

# =============================================================================
# Utility Tools
# =============================================================================
# CSV to Binary Converter Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/csv_to_binary_converter.cpp")
    add_executable(csv_to_binary_converter tools/csv_to_binary_converter.cpp)
    target_link_libraries(csv_to_binary_converter PRIVATE online_common)
    message(STATUS "Created csv_to_binary_converter tool")
endif()

# Dataset Analysis Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/analyze_dataset.cpp")
    add_executable(analyze_dataset tools/analyze_dataset.cpp)
    target_link_libraries(analyze_dataset PRIVATE online_common)
    message(STATUS "Created analyze_dataset tool")
endif()

# =============================================================================
# Unit Tests (optional)
# =============================================================================
if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        
        # Framework tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_framework_test.cpp")
            add_executable(test_framework_tests
                tests/test_framework_test.cpp
            )
            target_link_libraries(test_framework_tests
                PRIVATE
                    online_testing_framework
                    GTest::gtest_main
            )
            add_test(NAME TestFrameworkTests COMMAND test_framework_tests)
        endif()
        
        # Dynamic PSM Tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_dynamic_hysteresis.cpp")
            add_executable(test_dynamic_hysteresis
                tests/test_dynamic_hysteresis.cpp
            )
            target_link_libraries(test_dynamic_hysteresis
                PRIVATE
                    online_backend
                    online_strategy
                    online_common
                    GTest::gtest_main
            )
            add_test(NAME DynamicHysteresisTests COMMAND test_dynamic_hysteresis)
        endif()
        
        message(STATUS "Testing framework enabled with GTest")
    else()
        message(STATUS "GTest not found - skipping testing targets")
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================
# Add quote simulation module
# =============================================================================
add_subdirectory(quote_simulation)

# =============================================================================
install(TARGETS online_testing_framework online_learning online_strategy online_backend online_common quote_simulation
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "========================================")
message(STATUS "Online Trader Configuration Summary:")
message(STATUS "  - Eigen3: ${EIGEN3_VERSION}")
message(STATUS "  - Online Learning: ENABLED")
message(STATUS "  - Ensemble PSM: ENABLED")
message(STATUS "  - Strategy Framework: ENABLED")
message(STATUS "  - Testing Framework: ENABLED")
message(STATUS "  - Quote Simulation: ENABLED")
message(STATUS "  - MarS Integration: ENABLED")
message(STATUS "========================================")
