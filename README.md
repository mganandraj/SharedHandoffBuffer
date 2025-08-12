# SharedHandoffBuffer

A modern, Windows-based inter-process communication (IPC) library and demonstration application using shared memory, mutexes, and events for synchronized communication between processes. Built with modern C++11/14 features including RAII, move semantics, and template-based design.

## Overview

SharedHandoffBuffer provides a robust, type-safe mechanism for bidirectional communication between two processes (Source and Target) using Windows IPC primitives. The library ensures synchronized message passing with proper handshaking, liveness checking, and automatic resource management through RAII principles.

## Features

- **Modern C++ Design**: RAII-based resource management with automatic cleanup
- **Template-Based Architecture**: Generic `MappedBuffer<T>` for type-safe shared memory
- **RAII Handle Management**: `HandleWrapper` class for automatic Windows handle cleanup
- **Thread-Safe Operations**: `MutexGuard` RAII class for scoped mutex locking
- **Large Buffer Support**: 4KB payload buffer (configurable via template)
- **Event-Driven Signaling**: Windows events for process coordination
- **Robust Error Handling**: Comprehensive error checking with debug support
- **Liveness Checking**: Built-in mechanism to verify target process health
- **Command-Response Pattern**: Structured communication with type-safe enums
- **Automatic Process Management**: Parent process can launch and monitor child processes
- **Move Semantics**: Efficient resource transfer with move constructors/assignment

## Architecture

### Core Components

- **HandoffMessage Structure**: Contains command type, response type, and large payload buffer (4KB default)
- **SharedHandoffBuffer Class**: Main IPC wrapper providing high-level communication methods
- **HandleWrapper Class**: RAII wrapper for Windows handles (mutexes, events) with automatic cleanup
- **MutexGuard Class**: RAII scoped lock for thread-safe operations
- **MappedBuffer Template**: Generic template class for type-safe shared memory management

### Command Types
- `GetIdentity`: Request identity/process information
- `SetHandoffId`: Set a handoff identifier
- `LivenessCheck`: Verify target process is responsive

### Response Types
- `None`: No response (default state)
- `Identity`: Response containing identity information
- `Acknowledged`: Confirmation of command receipt
- `Alive`: Confirmation of liveness check

### IPC Objects Used

- **File Mapping**: Template-based shared memory buffer (`MappedBuffer<HandoffMessage>`)
- **Mutex**: Exclusive access to shared buffer with RAII locking (`MutexGuard`)
- **Events**: Signaling between source and target processes via `HandleWrapper`
  - Source Event: Target → Source notifications
  - Target Event: Source → Target notifications  
  - Target Ready Event: Target initialization signal with timeout support

## Building

### Prerequisites

- Visual Studio 2019 or later
- Windows SDK
- C++11 or later support (C++14 recommended for move semantics)

### Build Instructions

1. Open `SharedHandoffBuffer.sln` in Visual Studio
2. Select your preferred configuration (Debug/Release) and platform (x64/x86)
3. Build the solution (Ctrl+Shift+B)

Alternatively, use MSBuild from command line:
```cmd
msbuild SharedHandoffBuffer.sln /p:Configuration=Release /p:Platform=x64
```

## Usage

The application can run in three modes:

### Parent Mode (Default)
```cmd
SharedHandoffBuffer.exe
```
Automatically launches both Source and Target processes with a unique prefix.

### Source Process
```cmd
SharedHandoffBuffer.exe source <prefix>
```
Creates IPC objects and sends commands to the target.

### Target Process  
```cmd
SharedHandoffBuffer.exe target <prefix>
```
Connects to existing IPC objects and responds to source commands.

## Example Output

**Parent Process:**
```
[Parent] Both processes launched with prefix: HandoffBuffer123456789
[Parent] Both children exited. Done.
```

**Source Process:**
```
[Source] Waiting for target to be ready...
[Source] Target ready. Starting commands...
[Source] Target responded: Processed: Message #0
[Source] Target responded: Processed: Message #1
[Source] Target responded: Processed: Message #2
[Source] Polling target for liveness...
[Source] Target is alive.
```

**Target Process:**
```
[Target] Ready and waiting for commands...
[Target] Responded to Data command: Processed: Message #0
[Target] Responded to Data command: Processed: Message #1
[Target] Responded to Data command: Processed: Message #2
[Target] Responded to Liveness Check.
```

## API Reference

### SharedHandoffBuffer Constructor
```cpp
SharedHandoffBuffer(bool isSource, const std::string& sz_prefix)
```
- `isSource`: true for source (creates IPC objects), false for target (opens existing objects)
- `sz_prefix`: unique identifier for IPC object names

### Key Methods

#### Source Operations
- `WaitForTargetReady()`: Wait for target process initialization (with timeout)
- `SendFromSource(HandoffCommand cmd, const std::string& payload)`: Send command to target
- `WaitForTarget(HandoffResponse& resp, std::string& payload, DWORD timeout)`: Wait for target response

#### Target Operations
- `SignalTargetReady()`: Signal initialization complete
- `WaitForSource(std::string& payload)`: Wait for source command
- `SendFromTarget(HandoffResponse resp, const std::string& payload)`: Send response to source

## Error Handling

The library uses modern C++ exception safety and RAII principles:
- Throws `std::runtime_error` exceptions for IPC failures
- Calls `std::abort()` and `DebugBreak()` for critical errors in debug builds
- Automatic resource cleanup through destructors
- Move semantics prevent resource duplication
- Template-based design ensures type safety

Common error scenarios:
- Failed IPC object creation/opening
- Memory mapping failures  
- Handle access errors
- Timeout conditions

## Limitations

- Windows-specific implementation (uses Win32 API)
- Large payload size (4KB default, configurable via template)
- Designed for two-process communication
- Requires unique prefix for each process pair
- Debug builds include additional error checking with abort/breakpoint calls

## Modern C++ Features

### RAII Resource Management
- **HandleWrapper**: Automatic Windows handle cleanup with move semantics
- **MutexGuard**: Scoped mutex locking with exception safety
- **MappedBuffer<T>**: Template-based shared memory with automatic unmapping

### Template-Based Design
- Generic `MappedBuffer<T>` allows different message types
- Compile-time buffer size configuration
- Type-safe enum classes for commands and responses

### Exception Safety
- Strong exception safety guarantees
- Automatic resource cleanup on stack unwinding
- No resource leaks even on early termination

### Move Semantics
- Efficient resource transfer between objects
- Deleted copy constructors to prevent accidental duplication
- Move constructors and assignment operators for performance

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is available under the MIT License. See LICENSE file for details.

## Technical Notes

- Uses named objects with "Local\\" prefix for session isolation
- RAII-based automatic cleanup of Windows handles in destructors
- Thread-safe operations through scoped mutex locking (`MutexGuard`)
- Template-based design for type safety and flexibility
- Move semantics support for efficient resource management
- Supports timeout-based operations for robust error handling
- Debug builds include comprehensive error checking and diagnostics
- Modern C++11/14 features throughout the codebase
