defmodule ExQuic.MixProject do
  use Mix.Project

  def project do
    [
      app: :ex_quic,
      version: "0.1.0",
      elixir: "~> 1.10",
      start_permanent: Mix.env() == :prod,
      compilers: [:unifex, :bundlex] ++ Mix.compilers(),
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp deps do
    [
      {:unifex, "~> 0.3.3"},
      {:ex_doc, "~> 0.23", only: :dev, runtime: false},
      {:dialyxir, "~> 1.0", only: :dev, runtime: false},
      {:credo, "~> 1.5", only: :dev, runtime: false}
    ]
  end
end
