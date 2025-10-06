#!/bin/bash
# Online Trader Build Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Online Trader Build Script ===${NC}"

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: CMake not found. Install with: brew install cmake${NC}"
    exit 1
fi

if ! brew list eigen &> /dev/null; then
    echo -e "${RED}Error: Eigen3 not found. Install with: brew install eigen${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All required dependencies found${NC}"

# Parse command line arguments
BUILD_TYPE=${1:-Release}
CLEAN_BUILD=${2:-false}

if [ "$CLEAN_BUILD" = "clean" ]; then
    echo -e "\n${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "\n${YELLOW}Configuring CMake (Build Type: $BUILD_TYPE)...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

# Build
echo -e "\n${YELLOW}Building project...${NC}"
NPROC=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
cmake --build . -j$NPROC

echo -e "\n${GREEN}=== Build Complete ===${NC}"
echo -e "\nExecutables built:"
echo -e "  - ${GREEN}sentio_cli${NC}           (Main CLI for online learning)"
echo -e "  - ${GREEN}test_online_trade${NC}    (Online learning test tool)"
echo -e "  - ${GREEN}csv_to_binary_converter${NC} (Data conversion)"
if [ -f "analyze_dataset" ]; then
    echo -e "  - ${GREEN}analyze_dataset${NC}      (Dataset analysis)"
fi

echo -e "\n${YELLOW}Next steps:${NC}"
echo -e "  1. Copy market data to data/ directory"
echo -e "  2. Run: ${GREEN}./build/sentio_cli online-sanity-check --config config/enhanced_psm_config.json${NC}"
echo -e "  3. Run: ${GREEN}./build/test_online_trade --data data/futures.bin${NC}"

# Optional: Run tests if built
if [ -f "test_framework_tests" ]; then
    echo -e "\n${YELLOW}Running tests...${NC}"
    ctest --output-on-failure
fi
