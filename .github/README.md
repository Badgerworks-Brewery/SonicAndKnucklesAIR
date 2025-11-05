# GitHub Actions Workflows

This directory contains GitHub Actions workflows for automated building and testing of Sonic 3 A.I.R.

## Workflows

### 1. `build.yml` - Multi-Platform Build Workflow

**Purpose**: Comprehensive build workflow that compiles Sonic 3 A.I.R. for multiple platforms.

**Triggers**:
- Push to `main`, `master`, or `develop` branches
- Pull requests to `main`, `master`, or `develop` branches  
- Manual workflow dispatch

**Platforms**:
- **Linux** (Ubuntu Latest)
- **Windows** (Windows Latest with Visual Studio 2022)
- **macOS** (macOS Latest)

**Build Configurations**:
- Release
- Debug

**Features**:
- Installs all required dependencies for each platform
- Uses CMake build system
- Builds both Sonic3AIR and OxygenApp executables
- Uploads build artifacts for Release builds
- Provides build summary across all platforms

**Artifacts**: 
- Linux: `sonic3air_linux`, `oxygenapp_linux`
- Windows: `.exe` files
- macOS: `Sonic3AIR`, `OxygenApp`

### 2. `ci.yml` - Continuous Integration Workflow

**Purpose**: Quick validation build for continuous integration.

**Triggers**:
- Push to `main` or `master` branches
- Pull requests to `main` or `master` branches

**Platform**: Linux only (Ubuntu Latest)

**Features**:
- Fast build verification
- Release configuration only
- Verifies executable creation
- No artifact upload (focused on validation)

## Build Requirements

### Linux Dependencies
The workflows automatically install these required packages:
- `g++` - GNU C++ compiler
- `cmake` - Build system
- `libgl1-mesa-dev` - OpenGL development files
- `libglu1-mesa-dev` - OpenGL utility library
- `libasound2-dev` - ALSA sound library
- `libpulse-dev` - PulseAudio development files
- `libxcomposite-dev` - X11 Composite extension
- `libxxf86vm-dev` - X11 XFree86 video mode extension
- `libcurl4-openssl-dev` - HTTP client library

### Build Configuration
The workflows use these CMake options:
- `CMAKE_BUILD_TYPE`: Release or Debug
- `BUILD_OXYGEN_ENGINEAPP=ON`: Build the Oxygen Engine application
- `BUILD_OXYGEN_SERVER=OFF`: Skip server build (not needed for CI)
- `BUILD_SDL_STATIC=ON`: Use static SDL2 linking
- `USE_DISCORD=OFF`: Disable Discord integration (Linux/macOS)
- `USE_DISCORD=ON`: Enable Discord integration (Windows)
- `USE_IMGUI=ON`: Enable ImGui for development tools

## Usage

### Automatic Triggers
The workflows run automatically on:
- Code pushes to main branches
- Pull request creation/updates
- Manual trigger via GitHub Actions UI

### Manual Execution
1. Go to the "Actions" tab in the GitHub repository
2. Select the desired workflow
3. Click "Run workflow" button
4. Choose the branch and click "Run workflow"

### Downloading Artifacts
1. Navigate to the completed workflow run
2. Scroll to the "Artifacts" section
3. Download the desired platform build
4. Extract and use the executables

## Troubleshooting

### Common Issues

**Missing Dependencies**: If builds fail due to missing libraries, check that all required packages are listed in the workflow's dependency installation step.

**CMake Configuration Errors**: Verify that the CMake options match the project's requirements and that all paths are correct.

**Build Timeouts**: For large builds, consider adjusting the timeout settings or optimizing the build process.

**Platform-Specific Issues**: 
- Windows: Ensure Visual Studio components are properly configured
- macOS: Check Homebrew package availability
- Linux: Verify apt package names and versions

### Getting Help

If you encounter issues with the workflows:
1. Check the workflow run logs for detailed error messages
2. Compare with the manual build instructions in `Oxygen/sonic3air/build/`
3. Ensure all submodules are properly initialized
4. Verify that the repository structure matches expectations

## Customization

To modify the workflows:
1. Edit the `.yml` files in this directory
2. Test changes on a fork before merging
3. Consider the impact on build times and resource usage
4. Update this documentation when making significant changes