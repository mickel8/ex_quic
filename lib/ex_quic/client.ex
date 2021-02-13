defmodule ExQuic.Client do
  @moduledoc """
  Module responsible for connecting, sending and receiving QUIC messages.

  It works in client mode.

  Example usage
  ```
  defmodule ExampleClient do
    use ExQuic.Client

    @impl true
    def handle_quic_payload({:quic_payload, _payload} = msg) do
      IO.inspect(msg)
      :ok
    end
  end

  {:ok, pid} = Example.start_link(remote_ip: "127.0.0.3", remote_port: 8989)
  Example.send_payload(pid, "hello")
  ```
  """

  @callback handle_quic_payload(payload :: {:quic_payload, binary()}) :: :ok

  defmacro __using__(_opts) do
    quote location: :keep do
      @behaviour ExQuic.Client
      use GenServer
      require Unifex.CNode

      @impl true
      def handle_quic_payload({:quic_payload, payload}), do: :ok

      defoverridable handle_quic_payload: 1

      @spec start_link(remote_ip: binary(), remote_port: 0..65535) :: {:ok, pid()}
      def start_link(opts) do
        GenServer.start_link(__MODULE__, opts)
      end

      @spec send_payload(pid(), binary()) :: :ok
      def send_payload(pid, payload) do
        GenServer.call(pid, {:send_payload, payload})
      end

      @impl true
      def init(opts) do
        {:ok, cnode} = Unifex.CNode.start_link(:client)
        :ok = Unifex.CNode.call(cnode, :init, [opts[:remote_ip], opts[:remote_port]])
        {:ok, %{:cnode => cnode}}
      end

      @impl true
      def handle_call({:send_payload, payload}, _from, state) do
        Unifex.CNode.call(state.cnode, :send_payload, [payload])
        {:reply, :ok, state}
      end

      @impl true
      def handle_info({:quic_payload, _payload} = msg, state) do
        handle_quic_payload(msg)
        {:noreply, state}
      end
    end
  end
end
