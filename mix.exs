defmodule ExQuic.MixProject do
  use Mix.Project

  @version "0.1.2"
  @github_url "https://github.com/mickel8/ex_quic"

  def project do
    [
      app: :ex_quic,
      version: @version,
      elixir: "~> 1.10",
      start_permanent: Mix.env() == :prod,
      compilers: Mix.compilers() ++ [:third_party, :unifex, :bundlex],
      deps: deps(),

      # hex
      description: "Elixir wrapper over lsquic",
      package: package(),

      # docs
      name: "ExQuic",
      source_url: @github_url,
      docs: docs()
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

  defp package do
    [
      licenses: ["MIT"],
      links: %{"GitHub" => @github_url},
      files: [
        "lib",
        "mix.exs",
        "README*",
        "LICENSE*",
        ".formatter.exs",
        "bundlex.exs",
        "c_src"
      ]
    ]
  end

  defp docs do
    [
      main: "readme",
      extras: ["README.md", "LICENSE"],
      source_ref: "v#{@version}",
      nest_modules_by_prefix: [ExQuic]
    ]
  end
end
