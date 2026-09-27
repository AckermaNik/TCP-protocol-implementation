# microTCP: A TCP-Like Transport over UDP

This project implements a teaching-oriented, TCP-like reliable transport on top of UDP. It defines its own connection setup and teardown, packet header, sequence and acknowledgement numbers, checksum checks, receive-window flow control, congestion-window behavior, and retransmission attempts. A command-line bandwidth test can transfer a file using either the system's TCP sockets or this microTCP implementation.

This is an academic prototype, not a drop-in replacement for TCP and not suitable for production or untrusted networks. Its wire format and behavior are project-specific, and the implementation has known reliability and portability limitations noted below.

## Protocol architecture

The application-facing API is declared in `lib/microtcp.h`:

| Function | Role |
|---|---|
| `microtcp_socket` | Creates the underlying UDP socket and wraps it in microTCP state. |
| `microtcp_bind` | Binds the UDP socket and puts the endpoint into its listening state. |
| `microtcp_connect` | Initiates a connection using a SYN, SYN+ACK, ACK exchange. |
| `microtcp_accept` | Waits for the peer's SYN and completes the server side of the handshake. |
| `microtcp_send` | Segments application data, sends packet headers and payload datagrams, and waits for acknowledgements. |
| `microtcp_recv` | Receives and validates data, places it in the caller's buffer, acknowledges accepted data, and updates receive-window accounting. |
| `microtcp_shutdown` | Exchanges FIN/ACK control packets and closes the underlying UDP socket. |

The custom socket state (`microtcp_sock_t`) wraps a UDP descriptor and tracks connection state, peer address, sequence/acknowledgement positions, receive-buffer usage, congestion and receive windows, and packet/byte counters. The implementation uses UDP's `sendto` and `recvfrom`; it does not call the operating system's TCP transport for microTCP data.

## Packet format and transfer behavior

Each protocol packet starts with a packed 32-byte `microtcp_header_t`. Its fields carry sequence and acknowledgement numbers, control flags (`ACK`, `RST`, `SYN`, `FIN`), an advertised window, payload length, reserved/future-use values, and a CRC-32 checksum. Data is sent separately from its header in UDP datagrams. Payload segments are sized around the configured 1,400-byte MSS; the test application reads files in 4,096-byte chunks. The receiver buffer/window is configured as 8,192 bytes.

The sender limits a batch using the minimum of the peer's advertised receive window, the congestion window, and the remaining data. The receiver checks sequence progression and the payload CRC, copies accepted payload into the application buffer, advances its acknowledgement position, and advertises remaining buffer capacity. Duplicate or out-of-order data is intended to elicit duplicate acknowledgements so the sender can recover missing data.

The congestion logic is a simplified TCP-inspired scheme: it increases the congestion window during slow start and congestion avoidance, and reduces the threshold/window after timeout or repeated duplicate acknowledgements. A receive timeout triggers retransmission logic; three duplicate ACKs trigger a fast-retransmit path. This is an educational approximation—there is no claim of standards-compliant TCP congestion control or full TCP reliability semantics.

Connection setup follows a three-message SYN exchange. Shutdown uses FIN and ACK messages, with the client/server taking different branches depending on whether the peer has already initiated closing. The socket state enum in the header records states such as `LISTEN`, `ESTABLISHED`, `CLOSING_BY_PEER`, `CLOSING_BY_HOST`, `CLOSED`, and `INVALID`.

## End-to-end file-transfer workflow

`test/bandwidth_test.c` is the example application and comparison harness:

1. Start it in server mode with `-s`, a port (`-p`), and an output file (`-f`).
2. The server creates either a normal TCP endpoint or a UDP-backed microTCP endpoint depending on `-m`. The microTCP server binds, waits in `microtcp_accept`, and then repeatedly calls `microtcp_recv`.
3. Start a client with the same port, the server address (`-a`), and a source file (`-f`). In microTCP mode, the client calls `microtcp_connect`, reads the source file, passes chunks to `microtcp_send`, and then initiates shutdown.
4. The receiver writes delivered bytes to the output file and reports bytes received, transfer duration, and measured throughput.

Without `-m`, the harness uses standard TCP `socket`/`bind`/`listen`/`accept`/`send`/`recv` calls as a baseline. With `-m`, it uses the microTCP API. Run a server and client in separate terminals on a Unix-like system with GCC and compatible sockets APIs:

```sh
make -C test
```

Example invocation shape (the microTCP case is subject to the client-address caveat below):

```sh
# Server terminal
./test/band -s -m -p 8080 -f test/received.mp4

# Client terminal
./test/band -m -p 8080 -a 127.0.0.1 -f test/test1.mp4
```

Omit `-m` on both sides to run the TCP baseline. Both ends must use the same mode. `-a` is intended to be the server IPv4 address and is ignored in server mode.

## Repository layout

- `lib/microtcp.h` defines protocol constants, socket states, socket/header structures, and the public API.
- `lib/microtcp.c` implements UDP socket setup, the handshake, data transfer, retransmission paths, receive processing, shutdown, and the window-selection helper.
- `utils/crc32.h` provides the CRC-32 routine used to check data payloads.
- `utils/log.h` defines logging macros; debug logging is enabled by default.
- `test/bandwidth_test.c` contains server/client file-transfer paths for both standard TCP and microTCP and prints basic throughput statistics.
- `test/Makefile` compiles the implementation and test harness into `test/band`.
- `test/*.mp4` are binary transfer samples and generated outputs; `test/*.o` and `test/band` are generated build artifacts.
- `.vscode/` contains editor configuration. `report_hy335a-2.pdf` is a project report.

## Current limitations to keep in mind

- In `client_microtcp`, the destination address is currently assigned from `INADDR_ANY` instead of the parsed `-a` server address. Therefore the microTCP client does not reliably target the requested host; correct this before using it beyond a local experiment.
- The code is Linux/POSIX-oriented (`sendto`, `recvfrom`, `clock_gettime`, and Unix-style build commands). The included test harness uses `CLOCK_MONOTONIC_RAW`, which is not portable to every platform.
- The header is packed, but sequence, acknowledgement, window, and checksum fields are not consistently converted between host and network byte order. Interoperability across architectures is not guaranteed.
- The send/receive logic is synchronous and uses fixed-size buffers/chunks. Several recovery and accounting paths are fragile, so successful compilation or a local transfer is not proof of correctness under packet loss, reordering, larger workloads, or hostile input.
- The test harness's file-reading loops treat a zero-byte read as an error; exact-multiple-of-chunk file sizes may therefore need attention. The microTCP receive routine also relies on its fixed chunk behavior rather than consistently honoring the caller's requested length.
- CRC-32 detects accidental payload corruption; it does not authenticate a peer or protect against malicious packet modification.

Use this code to study transport-protocol mechanisms and their interactions. For real applications, use the operating system's TCP stack or a mature transport library.
