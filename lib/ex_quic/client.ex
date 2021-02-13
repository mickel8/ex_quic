defmodule ExQuic.Client do
  @moduledoc """
  Module responsible for connecting, sending and receiving QUIC messages.

  It works in client mode.

  Example usage
  ```
  defmodule ExampleClient do
    @behaviour ExQuic.Client

    @impl true
    def handle_quic_payload({:quic_payload, _payload} = msg) do
      IO.inspect(msg)
      :ok
    end
  end

  {:ok, pid} = ExQuic.Client.start_link(ExampleClient, remote_ip: "127.0.0.3", remote_port: 8989)
  ExQuic.Client.send_payload(pid, "hello")
  ```
  """
  use GenServer
  require Unifex.CNode

  @callback handle_quic_payload(payload :: {:quic_payload, binary()}) :: :ok

  @spec start_link(module(), remote_ip: binary(), remote_port: 0..65535) :: {:ok, pid()}
  def start_link(module, opts) do
    GenServer.start_link(__MODULE__, [caller: module] ++ opts)
  end

  @spec send_payload(pid(), binary()) :: :ok
  def send_payload(pid, payload) do
    GenServer.call(pid, {:send_payload, payload})
  end

  @impl true
  def init(opts) do
    {:ok, cnode} = Unifex.CNode.start_link(:client)
    :ok = Unifex.CNode.call(cnode, :init, [opts[:remote_ip], opts[:remote_port]])
    {:ok, %{:caller => opts[:caller], :cnode => cnode}}
  end

  @impl true
  def handle_call({:send_payload, payload}, _from, state) do
    Unifex.CNode.call(state.cnode, :send_payload, [payload])
    {:reply, :ok, state}
  end

  @impl true
  def handle_info({:quic_payload, _payload} = msg, %{caller: module} = state) do
    module.handle_quic_payload(msg)
    {:noreply, state}
  end
end
