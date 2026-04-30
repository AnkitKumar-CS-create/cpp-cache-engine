# Mini Redis C++17

A portfolio-quality mini Redis-style in-memory key-value server written in C++17.

This project implements a TCP database server with Redis-inspired commands, RESP request/response support, lazy key expiration, optional append-only persistence, and basic multi-client concurrency. It is intentionally smaller than Redis, but it is built to demonstrate real backend and systems programming concepts in clean, beginner-friendly C++.

## What This Project Demonstrates

- TCP socket programming with `socket`, `bind`, `listen`, `accept`, `recv`, and `send`
- Protocol parsing for both simple text commands and a RESP array subset
- RESP-style response formatting
- Command dispatch separated from networking
- In-memory storage with `std::unordered_map`
- Lazy expiration using `std::chrono`
- Append-only file persistence and replay
- Multi-client handling with `std::thread`
- Shared-state protection with `std::mutex` and `std::scoped_lock`
- CMake-based C++17 project organization
- Lightweight unit tests using `assert` and CTest

## Features

- Configurable TCP port with `--port`
- Optional append-only persistence with `--aof`
- Simple manual command input:

```text
SET name Slava
GET name
```

- RESP array request input:

```text
*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nSlava\r\n
```

- RESP-style responses:

```text
+OK\r\n
$5\r\nSlava\r\n
:1\r\n
$-1\r\n
```

- One thread per connected client
- Thread-safe key-value store and append-only log
- Unit tests for parser, command handling, storage, expiration, persistence, and concurrent increments

## Supported Commands

| Command | Arguments | Response | Description |
| --- | --- | --- | --- |
| `PING` | none | `+PONG\r\n` | Health check |
| `SET` | `key value` | `+OK\r\n` | Stores a string value |
| `GET` | `key` | bulk string or `$-1\r\n` | Reads a value |
| `DEL` | `key` | integer | Deletes a key, returns `1` or `0` |
| `EXISTS` | `key` | integer | Checks whether a key exists |
| `INCR` | `key` | integer or error | Increments an integer value |
| `EXPIRE` | `key seconds` | integer | Adds a relative expiration |
| `TTL` | `key` | integer | Returns remaining TTL, `-1`, or `-2` |

## Project Structure

```text
.
├── CMakeLists.txt
├── include/
│   ├── aof_log.hpp
│   ├── command.hpp
│   ├── command_handler.hpp
│   ├── command_parser.hpp
│   ├── key_value_store.hpp
│   ├── resp_response.hpp
│   └── tcp_server.hpp
├── src/
│   ├── aof_log.cpp
│   ├── command_handler.cpp
│   ├── command_parser.cpp
│   ├── key_value_store.cpp
│   ├── main.cpp
│   ├── resp_response.cpp
│   └── tcp_server.cpp
├── tests/
│   ├── test_aof_log.cpp
│   ├── test_command_handler.cpp
│   ├── test_command_parser.cpp
│   └── test_key_value_store.cpp
└── docs/
    └── architecture.md
```

## Build

On this Windows setup with MinGW:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=C:/mingw-w64/mingw64/bin/g++.exe
cmake --build build
```

On another machine with a configured C++17 compiler, this may be enough:

```bash
cmake -S . -B build
cmake --build build
```

## Run

Without persistence:

```powershell
.\build\miniredis.exe --port 6379
```

With append-only persistence:

```powershell
.\build\miniredis.exe --port 6379 --aof data.aof
```

## Windows PowerShell Manual Testing

Start the server in one PowerShell window:

```powershell
.\build\miniredis.exe --port 6379 --aof data.aof
```

Open a client in another PowerShell window:

```powershell
$client = [System.Net.Sockets.TcpClient]::new("127.0.0.1", 6379)
$stream = $client.GetStream()
$buffer = New-Object byte[] 1024
```

Send simple text commands:

```powershell
$cmd = "PING`r`nSET name Slava`r`nGET name`r`nEXISTS name`r`nDEL name`r`nGET name`r`n"
$bytes = [Text.Encoding]::ASCII.GetBytes($cmd)
$stream.Write($bytes, 0, $bytes.Length)
$count = $stream.Read($buffer, 0, $buffer.Length)
[Text.Encoding]::ASCII.GetString($buffer, 0, $count)
```

Expected response shape:

```text
+PONG
+OK
$5
Slava
:1
:1
$-1
```

Close the client:

```powershell
$client.Close()
```

## RESP Request Example

The server also accepts RESP arrays of bulk strings. This sends `SET name Slava`:

```powershell
$client = [System.Net.Sockets.TcpClient]::new("127.0.0.1", 6379)
$stream = $client.GetStream()
$cmd = "*3`r`n`$3`r`nSET`r`n`$4`r`nname`r`n`$5`r`nSlava`r`n"
$bytes = [Text.Encoding]::ASCII.GetBytes($cmd)
$stream.Write($bytes, 0, $bytes.Length)

$buffer = New-Object byte[] 1024
$count = $stream.Read($buffer, 0, $buffer.Length)
[Text.Encoding]::ASCII.GetString($buffer, 0, $count)
$client.Close()
```

Expected:

```text
+OK
```

## AOF Persistence Example

Run with persistence:

```powershell
.\build\miniredis.exe --port 6379 --aof data.aof
```

Send writes:

```text
SET name Slava
INCR count
INCR count
EXPIRE name 60
```

The file will contain simple line commands:

```text
SET name Slava
INCR count
INCR count
EXPIRE name 60
```

On restart, the server reads the file and replays the commands to rebuild the in-memory store.

## Architecture Overview

```text
TCP client
  -> TcpServer receives bytes
  -> CommandParser parses simple text or RESP input
  -> CommandHandler validates and executes commands
  -> KeyValueStore stores values and expiration metadata
  -> AofLog optionally persists successful mutations
  -> RespResponse formats output
  -> TcpServer sends bytes back to the client
```

See [docs/architecture.md](docs/architecture.md) for a fuller explanation.

## Request Lifecycle

1. `TcpServer` accepts a client socket.
2. A client thread reads bytes with `recv`.
3. Bytes are appended to a per-client pending buffer.
4. `CommandParser::try_parse` checks whether a full command is available.
5. The parser returns a `Command`.
6. `CommandHandler` validates argument counts and calls `KeyValueStore`.
7. Successful mutating commands are appended to `AofLog` when persistence is enabled.
8. `RespResponse` creates a RESP wire response.
9. `TcpServer` writes the response with `send`.

## Concurrency Model

The server uses one `std::thread` per connected client. `TcpServer` owns those threads in a `std::vector<std::thread>` and joins joinable threads during `stop()`.

Shared state is protected:

- `KeyValueStore` uses a mutex around map operations.
- `AofLog` uses a mutex around file append/load operations.
- `TcpServer` uses a mutex around the client thread vector.

This model is easy to understand and works well for learning. At very high concurrency, a thread per client can become expensive because every connection consumes OS thread resources.

## Testing

Build and run tests:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests cover:

- Simple command parsing
- RESP request parsing
- Command handling
- Store operations
- Expiration behavior
- AOF append and replay
- Basic concurrent increments

## Limitations

- This is not full Redis.
- Only a small command set is implemented.
- Input supports simple line commands plus a RESP array-of-bulk-strings subset.
- Values are strings only.
- One-thread-per-client is simple but expensive at high concurrency.
- Shutdown is improved, but not a perfect production graceful shutdown.
- AOF grows forever because there is no compaction or rewrite.
- `EXPIRE` replay uses a relative TTL from startup, not the original absolute expiration time.
- No authentication.
- No clustering.
- No replication.
- No transactions.
- No pub/sub.
- No eviction policy.
- No snapshots.

## Future Improvements

- AOF compaction/rewrite
- Snapshot persistence
- Absolute expiration timestamps in persistence
- Thread pool instead of one thread per client
- Event loop and non-blocking I/O
- More Redis commands and data types
- Better graceful shutdown with signal handling and client socket tracking
- Benchmarks and load tests
- More robust integration tests with real TCP clients

## GitHub-Ready Details

Suggested repository name:

```text
mini-redis-cpp
```

Suggested GitHub About text:

```text
A C++17 Redis-inspired TCP key-value server with RESP parsing, TTLs, AOF persistence, and multi-client concurrency.
```

Suggested resume bullet points:

- Built a Redis-inspired in-memory key-value database in C++17 with TCP sockets, RESP parsing, TTL expiration, and append-only persistence.
- Implemented thread-safe multi-client command handling using `std::thread`, `std::mutex`, and clean separation between networking, parsing, storage, and persistence.
- Added CMake build automation and unit tests covering parser behavior, command execution, expiration, persistence replay, and concurrent increments.

Suggested portfolio description:

```text
Mini Redis C++17 is a systems/backend project that implements a Redis-style TCP key-value server from scratch. It supports simple text and RESP command input, RESP output, core string commands, lazy expiration, append-only persistence, startup replay, and multi-client access with mutex-protected shared state. The project emphasizes clean architecture, testability, and readable C++17.
```

