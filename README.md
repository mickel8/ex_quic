# ExQuic
[![Hex.pm](https://img.shields.io/hexpm/v/ex_quic.svg)](https://hex.pm/packages/ex_quic)
[![API Docs](https://img.shields.io/badge/api-docs-yellow.svg?style=flat)](https://hexdocs.pm/ex_quic/)

Elixir wrapper over [lsquic].

ExQuic wraps [lsquic] for usage in Elixir.
At this moment it provides very simple and limited client that is able to communicate with [echo_server].

Future plans:
* extend client implementation
* implement server side
* add support for ECN
* add support for HTTP/3

## Installation

To successfully include `ex_quic` in your project you need to install:
* cmake
* zlib

The package can be installed by adding `ex_quic` to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:ex_quic, "~> 0.1.0"}
  ]
end
```

# Architecture
ExQuic uses [bundlex] and [unifex] for compiling and spawning CNode that is responsible for doing
all QUIC stuff using [lsquic].

[lsquic]: https://github.com/litespeedtech/lsquic
[bundlex]: https://github.com/membraneframework/bundlex
[unifex]: https://github.com/membraneframework/unifex
[echo_server]: https://github.com/litespeedtech/lsquic/blob/master/bin/echo_server.c
