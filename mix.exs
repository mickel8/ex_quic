defmodule ExQuic.MixProject do
  use Mix.Project

  @version "0.2.0"
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
        "c_src/ex_quic/client.c",
        "c_src/ex_quic/client.h",
        "c_src/ex_quic/client.spec.exs",
        "c_src/ex_quic/lsquic_utils.c",
        "c_src/ex_quic/lsquic_utils.h",
        "c_src/ex_quic/libev/*.[ch]",
        "third_party/boringssl/**/*.[chS]",
        "third_party/boringssl/**/*.cc",
        "third_party/boringssl/**/CMakeLists.txt",
        "third_party/boringssl/**/sources.cmake",
        "third_party/boringssl/LICENSE",
        "third_party/lsquic/**/*.[ch]",
        "third_party/lsquic/**/CMakeLists.txt",
        "third_party/lsquic/**/*.pl",
        "third_party/lsquic/**/*.sh",
        "third_party/lsquic/LICENSE.chrome",
        "third_party/lsquic/LICENSE"
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
