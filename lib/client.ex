defmodule ExQuic.Client do
  use GenServer

  require Unifex.CNode

  def start_link() do
    GenServer.start_link(__MODULE__, [])
  end

  @impl true
  def init(_opts) do
    {:ok, cnode} = Unifex.CNode.start_link(:client)
    :ok = Unifex.CNode.call(cnode, :init, ["127.0.0.2", 7932, "127.0.0.3", 8989])
    {:ok, %{}}
  end
end
