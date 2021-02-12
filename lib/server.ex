defmodule ExQuic.Server do
  use GenServer

  require Unifex.CNode

  # Client API
  def start_link() do
    GenServer.start_link(__MODULE__, [])
  end

  def send_payload(pid, payload) do
    GenServer.call(pid, {:send_payload, payload})
  end

  # Server
  @impl true
  def init(_opts) do
    {:ok, cnode} = Unifex.CNode.start_link(:server)
    :ok = Unifex.CNode.call(cnode, :init, ["127.0.0.3", 8989])
    {:ok, %{:cnode => cnode}}
  end

  @impl true
  def handle_call({:send_payload, payload}, _from, state) do
    Unifex.CNode.call(state.cnode, :send_payload, [payload])
    {:reply, :ok, state}
  end

  @impl true
  def handle_info(msg, state) do
    IO.inspect(msg)
    {:noreply, state}
  end
end
