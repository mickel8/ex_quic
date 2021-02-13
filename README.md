# ExQuic
[![Hex.pm](https://img.shields.io/hexpm/v/ex_quic.svg)](https://hex.pm/packages/ex_quic)
[![API Docs](https://img.shields.io/badge/api-docs-yellow.svg?style=flat)](https://hexdocs.pm/ex_quic/)
[![CI](https://github.com/mickel8/ex_quic/workflows/CI/badge.svg)](https://github.com/mickel8/ex_quic/actions)

Elixir wrapper over [lsquic].

ExQuic wraps [lsquic] for usage in Elixir.
At this moment it provides very simple and limited client that is able to communicate with [echo_server].

Future plans:
* extend client implementation
* implement server side
* add support for ECN
* add support for HTTP/3

## Installation

At this moment the package cannot be installed in your project. 
The released versions doesn't work.
As soon as server side will be implemented the package will be released on hex.
However you can still download this repository and write some test code directly inside it.
Below is short instruction.

You will need:
* cmake (for building boringssl and lsquic)
* zlib (for lsquic)
* libevent-dev (for lsquic)

After cloning repository you need to download submodules and use supported `boringssl` commit.

```
git submodule update --init --recursive
cd third_party/boringssl
git checkout a2278d4d2cabe73f6663e3299ea7808edfa306b9
```

There is no need to compile `boringsssl` or `lsquic` on your own. 
It will be done during library compilation. 

That's it. Now you can test `ex_quic`.

## Quickstart
At this moment `ExQuic.Client` is able to communicate with [echo_server] written in C so to test it
you first need to compile and start `echo_server`.

Compiling `echo_server` can be done by cloning and compiling `lsquic`.

Starting `echo_server` can be done with following command.
```
./echo_server -c www.example.com,/path/to/server.cert,/path/to/server.key -s  127.0.0.3:8989
```

Now you can write your own client and send a message to server from elixir
```elixir
defmodule ExampleClient do
  @behaviour ExQuic.Client

  @impl true
  def handle_quic_payload({:quic_payload, _payload} = msg) do
    IO.inspect(msg)
    :ok
  end
end

{:ok, pid} = ExQuic.Client.start_link(ExampleClient, remote_ip: "127.0.0.3", remote_port: 8989)
ExQuic.Client.send_payload(pid, "hello\n")
```

`echo_server` expects the message to be end with `\n`

## Compilation warnings
There might be a lot of warnings during compilation related with `libev`.
They will be hopefully turned off in the future.
For more information please refer to `libev` [documentation](http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#COMPILER_WARNINGS)

## Architecture
ExQuic uses [bundlex] and [unifex] for compiling and spawning CNode that is responsible for doing
all QUIC stuff using [lsquic].

## Developing
To develop locally download submodules and use supported version of BoringSSL.

```
git submodule update --init --recursive
cd third_party/boringssl
git checkout a2278d4d2cabe73f6663e3299ea7808edfa306b9
```

[lsquic]: https://github.com/litespeedtech/lsquic
[bundlex]: https://github.com/membraneframework/bundlex
[unifex]: https://github.com/membraneframework/unifex
[echo_server]: https://github.com/litespeedtech/lsquic/blob/master/bin/echo_server.c
