## WinRPCResolver
A simple tool to resolve RPC endpoints to their respective UUIDs. This tool is useful for reverse engineering and debugging RPC services on windows based machines.

## Build Instructions
1. Clone the repository
```bash
git clone https://github.com/mendax0110/WinRPCResolver.git
```

2. Change directory to the cloned repository
```bash
cd WinRPCResolver
```

3. Create the build directory
```bash
mkdir build
```

4. Change directory to the build directory
```bash
cd build
```

5. Build CMake files
```bash
cmake ..
```

6. Build the project
```bash
cmake --build .
```

## Usage
```bash
WinRPCResolver.exe --gui
```

## Supported Platforms
- Windows
