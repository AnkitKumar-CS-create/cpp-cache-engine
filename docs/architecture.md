# Architecture

Mini Redis C++17 is organized around a small request pipeline. Each layer has one main job, which keeps the server understandable and testable.

## Components

### TcpServer

`TcpServer` owns the listening socket, accepts clients, reads bytes, and sends responses. It does not decide what commands mean. That work is delegated to the parser and handler.

Responsibilities:

- Initialize platform socket support on Windows.
- Bind and listen on the configured port.
- Accept client connections.
- Start one client thread per connection.
- Track client threads and join them during shutdown.
- Keep a per-client pending byte buffer.
- Send complete responses back to clients.

### CommandParser

`CommandParser` turns raw input into a `Command`.

It supports two input styles:

- Simple line input, such as `SET name Slava`.
- RESP arrays of bulk strings, such as `*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nSlava\r\n`.

For network input, `try_parse` returns one of three states:

- `Complete`: a full command was parsed.
- `Incomplete`: more bytes are needed.
- `Error`: the request is malformed.

This matters because TCP is a byte stream. One `recv` call might contain half a command, exactly one command, or several commands.

### CommandHandler

`CommandHandler` receives a parsed `Command`, validates the argument count, calls `KeyValueStore`, and returns a RESP-formatted string.

It implements:

- `PING`
- `SET`
- `GET`
- `DEL`
- `EXISTS`
- `INCR`
- `EXPIRE`
- `TTL`

When AOF persistence is enabled, it appends successful mutating commands:

- `SET`
- `DEL`
- `INCR`
- `EXPIRE`

Read-only commands are not logged.

### KeyValueStore

`KeyValueStore` owns the in-memory data:

```cpp
std::unordered_map<std::string, StoredValue>
```

Each stored value contains:

- the string value
- an optional expiration time

Expiration is lazy. A key is checked and removed when an operation touches it. There is no cleanup thread yet.

The store is protected by a mutex so multiple client threads can safely access it.

### RespResponse

`RespResponse` formats server replies using RESP-style output:

- Simple strings: `+OK\r\n`
- Errors: `-ERR message\r\n`
- Integers: `:1\r\n`
- Bulk strings: `$5\r\nSlava\r\n`
- Null bulk strings: `$-1\r\n`

Keeping formatting in one helper prevents protocol details from spreading through the codebase.

### AofLog

`AofLog` implements append-only persistence.

When enabled with `--aof PATH`, successful mutating commands are appended in simple line format:

```text
SET name Slava
INCR count
EXPIRE name 60
DEL oldkey
```

On startup, `TcpServer` loads those lines, parses them, and replays them through `CommandHandler::replay`. Replay avoids logging commands again, so startup does not duplicate the AOF file.

The AOF log uses a mutex to prevent concurrent client threads from interleaving writes.

## Request Flow

```text
client socket
  -> TcpServer::handle_client
  -> CommandParser::try_parse
  -> CommandHandler::handle
  -> KeyValueStore
  -> optional AofLog append
  -> RespResponse
  -> TcpServer::send_response
```

## Threading Model

The server uses one thread per client. This is straightforward and good for learning because each client connection has a simple blocking read loop.

Shared resources are protected:

- `KeyValueStore` protects the map and expiration metadata.
- `AofLog` protects file writes.
- `TcpServer` protects the client thread vector.

This model is not the final word in scalability. A production database server would usually use a thread pool, non-blocking I/O, or an event loop.

