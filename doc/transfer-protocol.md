# SMTool Transfer Protocol (STP) v1

## Status

Draft

Version: 1

## Purpose

The SMTool Transfer Protocol (STP) provides a simple mechanism for transferring captured ideas from the Android companion application to the desktop application over a local TCP connection.

The protocol is intentionally designed to be:

* Simple to understand
* Easy to debug
* Easy to implement in Qt/C++ and Kotlin
* Human-readable
* Independent of HTTP, gRPC, Protobuf, TLS, or external dependencies

Version 1 assumes a trusted local network and does not provide encryption or authentication beyond user confirmation.

---

# Transport

Transport: TCP

Default Port: TBD

Connection model:

* One connection transfers one payload.
* The desktop application acts as the server.
* The Android application acts as the client.
* The server closes the connection after sending the final result.

---

# Message Framing

Messages are newline-delimited JSON (NDJSON).

Each protocol message consists of:

```text
<json-object>\n
```

Example:

```text
{"type":"hello","app":"smtool-transfer","version":1,"code":"12345678"}\n
```

Only one JSON object is allowed per line.

---

# Limits

Maximum message size:

```text
4 MiB
```

Messages exceeding this limit MUST be rejected.

Maximum payload size:

```text
4 MiB
```

The server MAY use a lower limit.

---

# Timeouts

## Hello Timeout

After a TCP connection is established, the server waits:

```text
5 seconds
```

for a valid hello message.

If no valid hello message is received, the connection is closed.

## Payload Timeout

After the transfer is approved, the server waits:

```text
30 seconds
```

for the payload message.

If the timeout expires, the connection is closed.

---

# Protocol Flow

```text
Client connects

Client -> Hello

Server -> Continue

Client -> Payload

Server -> Result

Connection closed
```

---

# Message Types

## Hello

Sent by the client immediately after connecting.

### Example

```json
{
  "type": "hello",
  "app": "smtool-transfer",
  "version": 1,
  "code": "12345678"
}
```

### Fields

| Field   | Type    | Required | Description                   |
| ------- | ------- | -------- | ----------------------------- |
| type    | string  | yes      | Must be "hello"               |
| app     | string  | yes      | Protocol identifier           |
| version | integer | yes      | Protocol version              |
| code    | string  | yes      | Eight-digit confirmation code |

### Validation

The server MUST verify:

* type == "hello"
* app == "smtool-transfer"
* version == 1
* code consists of exactly eight digits

If validation fails, the server MUST send an Error message and close the connection.

---

# User Confirmation

After receiving a valid Hello message, the desktop application displays a confirmation dialog.

Example:

```text
Transfer request received

Code: 1234-5678

Accept transfer?
```

The user may choose:

* Accept
* Reject

---

# Continue

Sent by the server after user confirmation.

### Accepted

```json
{
  "type": "continue",
  "ok": true
}
```

### Rejected

```json
{
  "type": "continue",
  "ok": false,
  "message": "Transfer rejected by user"
}
```

After a rejection, the server closes the connection.

---

# Payload

Sent by the client only after receiving:

```json
{
  "type": "continue",
  "ok": true
}
```

The payload contains captured ideas.

### Example

```json
{
  "type": "ideas",
  "version": 1,
  "items": [
    {
      "title": "Blog post idea",
      "text": "Write about local-first software.",
      "created_at": "2026-06-16T15:30:00Z",
      "source": "android"
    }
  ]
}
```

### Fields

| Field   | Type    | Required |
| ------- | ------- | -------- |
| type    | string  | yes      |
| version | integer | yes      |
| items   | array   | yes      |

### Item Fields

| Field      | Type   | Required |
| ---------- | ------ | -------- |
| title      | string | yes      |
| text       | string | yes      |
| created_at | string | no       |
| source     | string | no       |

The desktop application passes the parsed payload to its internal import function.

---

# Result

Sent by the server after processing the payload.

## Success

```json
{
  "type": "result",
  "ok": true,
  "imported": 3
}
```

## Failure

```json
{
  "type": "result",
  "ok": false,
  "message": "No valid ideas found"
}
```

### Fields

| Field    | Type    | Required     |
| -------- | ------- | ------------ |
| type     | string  | yes          |
| ok       | boolean | yes          |
| imported | integer | success only |
| message  | string  | failure only |

The server closes the connection immediately after sending the Result message.

---

# Error Message

An Error message may be sent at any time if protocol rules are violated.

### Example

```json
{
  "type": "error",
  "message": "Expected hello message"
}
```

### Examples

* Invalid JSON
* Message too large
* Unsupported version
* Invalid state transition
* Timeout
* Unexpected message type

After sending an Error message, the server MUST close the connection.

---

# State Machine

Server states:

```text
CONNECTED
    |
    v
WAIT_HELLO
    |
    v
WAIT_USER_CONFIRMATION
    |
    +--> REJECTED -> CLOSED
    |
    v
WAIT_PAYLOAD
    |
    v
PROCESSING
    |
    v
RESULT_SENT
    |
    v
CLOSED
```

Any protocol violation transitions directly to:

```text
ERROR
  |
  v
CLOSED
```

---

# Security Considerations

Version 1 assumes:

* Trusted local network
* User physically present at the desktop machine
* Manual user confirmation

No encryption is provided.

No authentication is provided.

Future protocol versions may add:

* TLS
* Pairing
* Device registration
* Certificate-based authentication
* Automatic discovery
* Compression

These additions must remain backward-compatible where practical.

---

# Implementation Notes

Recommended Qt classes:

```cpp
QTcpServer
QTcpSocket
QJsonDocument
QJsonObject
```

Recommended Android classes:

```kotlin
ServerSocket
Socket
JSONObject
```

Messages should be processed line-by-line.

Implementations MUST NOT assume that a TCP read operation returns a complete message.

Messages must be buffered until a newline character is received.
