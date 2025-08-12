# SharedHandoffBuffer

A Windows-based inter-process communication (IPC) library and demonstration application using shared memory, mutexes, and events for synchronized communication between processes.

## Overview

SharedHandoffBuffer provides a robust mechanism for bidirectional communication between two processes (Source and Target) using Windows IPC primitives. The library ensures synchronized message passing with proper handshaking and liveness checking.

## Features

- **Shared Memory Communication**: Uses Windows file mapping for efficient data sharing
- **Thread-Safe Operations**: Mutex-based synchronization for concurrent access
- **Event-Driven Signaling**: Windows events for process coordination
- **Liveness Checking**: Built-in mechanism to verify target process health
- **Command-Response Pattern**: Structured communication with defined message types
- **Automatic Process Management**: Parent process can launch and monitor child processes

## Architecture

### Core Components

- **HandoffMessage Structure**: Contains command type, response type, and payload data (256 bytes)
- **SharedHandoffBuffer Class**: Main IPC wrapper providing high-level communication methods
- **Command Types**: 
  - `eData`: Send data payload
  - `eLivenessCheck`: Verify target process is responsive
- **Response Types**:
  - `ePayload`: Response containing processed data
  - `eAlive`: Confirmation of liveness check

### IPC Objects Used

- **File Mapping**: Shared memory buffer for message exchange
- **Mutex**: Exclusive access to shared buffer
- **Events**: Signaling between source and target processes
  - Source Event: Target → Source notifications
  - Target Event: Source → Target notifications  
  - Target Ready Event: Target initialization signal

## Building

### Prerequisites

- Visual Studio 2019 or later
- Windows SDK
- C++11 or later support

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
SharedHandoffBuffer(bool b_create_ipc, const std::string& sz_prefix)
```
- `b_create_ipc`: true for source (creates IPC objects), false for target (opens existing objects)
- `sz_prefix`: unique identifier for IPC object names

### Key Methods

#### Source Operations
- `WaitForTargetReady()`: Wait for target process initialization
- `SendFromSource(eHandoffCommand e_cmd, const std::string& sz_payload)`: Send command to target
- `WaitForTarget(eHandoffResponse& e_out_resp, std::string& sz_out_payload, DWORD dw_timeout)`: Wait for target response

#### Target Operations
- `SignalTargetReady()`: Signal initialization complete
- `WaitForSource(std::string& sz_out_payload)`: Wait for source command
- `SendFromTarget(eHandoffResponse e_resp, const std::string& sz_payload)`: Send response to source

## Error Handling

The library throws `std::runtime_error` exceptions for:
- Failed IPC object creation/opening
- Memory mapping failures
- Handle access errors

## Limitations

- Windows-specific implementation (uses Win32 API)
- Fixed payload size (256 bytes)
- Designed for two-process communication
- Requires unique prefix for each process pair

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
- Automatic cleanup of Windows handles in destructor
- Thread-safe operations through mutex synchronization
- Supports timeout-based operations for robust error handling
