defmodule ExQuicTest do
  use ExUnit.Case
  doctest ExQuic

  test "greets the world" do
    assert ExQuic.hello() == :world
  end
end
