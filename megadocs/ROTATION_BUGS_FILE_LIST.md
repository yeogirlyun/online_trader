# Rotation Trading Critical Bugs - Source File List

This document contains all source files related to the rotation trading critical bugs.

## Backend Components

- `src/backend/rotation_trading_backend.cpp`
- `include/backend/rotation_trading_backend.h`

## Strategy Components

- `src/strategy/rotation_position_manager.cpp`
- `include/strategy/rotation_position_manager.h`
- `src/strategy/signal_aggregator.cpp`
- `include/strategy/signal_aggregator.h`
- `src/strategy/multi_symbol_oes_manager.cpp`
- `include/strategy/multi_symbol_oes_manager.h`
- `src/strategy/online_ensemble_strategy.cpp`
- `include/strategy/online_ensemble_strategy.h`
- `src/strategy/signal_output.cpp`
- `include/strategy/signal_output.h`

## Learning Components

- `src/learning/online_predictor.cpp`
- `include/learning/online_predictor.h`

## CLI Components

- `src/cli/rotation_trade_command.cpp`
- `include/cli/rotation_trade_command.h`
- `src/cli/command_registry.cpp`

## Data Management

- `src/data/multi_symbol_data_manager.cpp`
- `include/data/multi_symbol_data_manager.h`

## Feature Engineering

- `src/features/unified_feature_engine.cpp`
- `include/features/unified_feature_engine.h`
- `include/features/rolling.h`

## Configuration

- `config/rotation_strategy.json`

## Common/Types

- `include/common/types.h`
- `src/common/utils.cpp`
- `include/common/utils.h`

## Build Configuration

- `CMakeLists.txt`
