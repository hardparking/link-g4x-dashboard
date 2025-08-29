#!/bin/bash

# Link G4X Dashboard Build Script
# This script provides convenient commands for building and managing the project

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Activate virtual environment
activate_venv() {
    if [ ! -d ".venv" ]; then
        print_error "Virtual environment not found. Run 'setup' first."
        exit 1
    fi
    source .venv/bin/activate
}

# Check if we're in the right directory
check_project() {
    if [ ! -f "platformio.ini" ]; then
        print_error "platformio.ini not found. Are you in the project directory?"
        exit 1
    fi
}

# Setup function
setup() {
    print_status "Setting up development environment..."
    
    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is required but not installed."
        exit 1
    fi
    
    # Create virtual environment if it doesn't exist
    if [ ! -d ".venv" ]; then
        print_status "Creating virtual environment..."
        python3 -m venv .venv
    fi
    
    # Activate and install PlatformIO
    print_status "Installing PlatformIO..."
    source .venv/bin/activate
    pip install platformio
    
    # Initialize project
    print_status "Initializing PlatformIO project..."
    pio project init --ide vscode
    
    print_success "Setup complete! You can now use './build.sh build' to compile the project."
}

# Build function
build() {
    check_project
    activate_venv
    
    print_status "Building Link G4X Dashboard..."
    pio run --environment esp32p4_pioarduino
    
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully!"
        print_status "Memory usage:"
        pio run --environment esp32p4_pioarduino --target size
    else
        print_error "Build failed!"
        exit 1
    fi
}

# Upload function
upload() {
    check_project
    activate_venv
    
    print_status "Uploading to device..."
    pio run --environment esp32p4_pioarduino --target upload
    
    if [ $? -eq 0 ]; then
        print_success "Upload completed successfully!"
    else
        print_error "Upload failed!"
        exit 1
    fi
}

# Monitor function
monitor() {
    check_project
    activate_venv
    
    print_status "Starting serial monitor..."
    pio device monitor --environment esp32p4_pioarduino
}

# Clean function
clean() {
    check_project
    activate_venv
    
    print_status "Cleaning build files..."
    pio run --target clean
    print_success "Clean completed!"
}

# Dependencies function
deps() {
    check_project
    activate_venv
    
    print_status "Installing/updating dependencies..."
    pio pkg install
    print_success "Dependencies updated!"
}

# Show help
show_help() {
    echo "Link G4X Dashboard Build Script"
    echo ""
    echo "Usage: ./build.sh [command]"
    echo ""
    echo "Commands:"
    echo "  setup     - Set up development environment (run this first)"
    echo "  build     - Compile the project"
    echo "  upload    - Upload firmware to device"
    echo "  monitor   - Start serial monitor"
    echo "  clean     - Clean build files"
    echo "  deps      - Install/update dependencies"
    echo "  help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build.sh setup     # First time setup"
    echo "  ./build.sh build     # Compile the project"
    echo "  ./build.sh upload    # Upload to M5Stack Tab5"
    echo "  ./build.sh monitor   # View serial output"
}

# Main script logic
case "$1" in
    setup)
        setup
        ;;
    build)
        build
        ;;
    upload)
        upload
        ;;
    monitor)
        monitor
        ;;
    clean)
        clean
        ;;
    deps)
        deps
        ;;
    help|--help|-h)
        show_help
        ;;
    "")
        print_warning "No command specified. Use './build.sh help' for usage information."
        show_help
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
