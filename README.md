*This project has been created as part of the 42 curriculum by &lt;ruortiz-&gt;, &lt;rmakende&gt;.*

# ft_irc

![Language](https://img.shields.io/badge/language-C%2B%2B98-00599C?logo=cplusplus&logoColor=white)
![Standard](https://img.shields.io/badge/RFC-1459%20%2F%202812-informational)
![School](https://img.shields.io/badge/42-cursus-000000)
![Status](https://img.shields.io/badge/status-in%20progress-yellow)

## Description

`ft_irc` is an IRC server written in C++98, non-blocking and single-threaded.
Its goal is to implement the subset of the IRC protocol (RFC 1459 / 2812)
required for real clients such as irssi or HexChat to connect, authenticate,
create channels and talk to each other, all handled from a single `poll()`
without forking and without blocking reads.

In broad terms, the server opens a TCP socket, accepts incoming connections,
accumulates each client's bytes until a full command is reconstructed
(terminated by `\r\n`), interprets it according to the IRC rules and either
replies or relays the message to the other members of the relevant channel.

The project is split into two halves developed in parallel that communicate
only through the shared `Client` object:

| Part | Responsibility | Files |
|---|---|---|
| **1 · Network & core** | socket, `bind`, `listen`, `fcntl`, `poll()`, client join/leave, async buffers | `src/Server.cpp`, `src/main.cpp` |
| **2 · IRC protocol** | parsing, `PASS`/`NICK`/`USER`, channels, `KICK` `INVITE` `TOPIC` `MODE` | `src/CommandHandler.cpp`, `src/Channel.cpp` |
| **Shared** | shared state and the two seams | `inc/Client.hpp`, `inc/ICommandHandler.hpp`, `inc/IServerContext.hpp` |

**[→ Interactive diagram of the Client contract](https://RaulMkn.github.io/Cursus42_irc/docs/irc-client-contract.html)**

## Instructions

### Requirements

- A compiler with C++98 support (`c++`, `clang++` or `g++`)
- `make`
- Linux or macOS

### Compilation

```bash
make            # builds the ircserv binary
make clean      # removes object files
make fclean     # removes object files and the binary
make re         # fclean + make
```

Built with `-Wall -Wextra -Werror -std=c++98`.

### Execution

```bash
./ircserv <port> <password>
```

## Usage

With a real client:

```bash
./ircserv 6667 hunter2
irssi -c 127.0.0.1 -p 6667 -w hunter2
```

And by hand with netcat, useful to check the handling of fragmented packets:

```bash
nc -C 127.0.0.1 6667
PASS hunter2
NICK bob
USER bob 0 * :Bob
JOIN #42madrid
PRIVMSG #42madrid :hello
```

## Features

Implemented commands:

`PASS` · `NICK` · `USER` · `QUIT` · `PING` · `JOIN` · `PART` · `PRIVMSG` ·
`NOTICE` · `TOPIC` · `KICK` · `INVITE` · `MODE` (`+i` `+t` `+k` `+o` `+l`)

Extended technical documentation (which layer writes each `Client` field, the
disconnection order and leak prevention) lives in
[`docs/client-contract.md`](docs/client-contract.md).

## Resources

Classic references on the protocol and the network programming used:

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol (ircdocs)](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man` pages for `poll`, `socket`, `bind`, `listen`, `accept`, `recv`, `send`, `fcntl`

### Use of AI

An AI tool was used as support on well-scoped tasks, always with manual review
of the result:

- **`Client` contract design**: drafting the shared interface between the
  network layer and the protocol layer (`inc/Client.hpp`,
  `inc/ICommandHandler.hpp`, `inc/IServerContext.hpp`) and its implementation
  (`src/Client.cpp`).
- **Documentation**: draft of `docs/client-contract.md` and generation of the
  interactive architecture diagram under `docs/`.
- **This README**: initial structure and wording.

The IRC protocol logic and the network layer (`poll()`, sockets) are
implemented and validated manually by the authors.

## Structure

```
.
├── inc/                    # headers, including the shared contract
├── src/                    # implementation
├── docs/                   # technical documentation and interactive diagram
└── Makefile
```
