defmodule Mix.Tasks.Compile.ThirdParty do
  use Mix.Task

  require Logger

  @ld_lib_paths [
    Path.expand("./third_party/boringssl/ssl"),
    Path.expand("./third_party/boringssl/crypto"),
    Path.expand("./third_party/lsquic/src/liblsquic")
  ]

  @impl true
  def run(_args) do
    Logger.info("Downloading repositories")
    System.cmd("git", ["submodule", "update", "--init", "--recursive"])

    Logger.info("Compiling boringssl")
    System.cmd("cmake", ["-DBUILD_SHARED_LIBS=1", "."], cd: "third_party/boringssl")
    System.cmd("make", [], cd: "third_party/boringssl")

    Logger.info("Compiling lsquic")

    System.cmd("cmake", ["-DLSQUIC_SHARED_LIB=1", "-DBORINGSSL_DIR=../boringssl", "."],
      cd: "third_party/lsquic"
    )

    System.cmd("make", [], cd: "third_party/lsquic")

    Logger.info("Adding compiled libs to LD_LIBRARY_PATH")
    System.put_env("LD_LIBRARY_PATH", get_ld_lib_path())
    {:ok, []}
  end

  defp get_ld_lib_path() do
    sys_ld_lib_path = System.get_env("LD_LIBRARY_PATH")

    if sys_ld_lib_path == nil,
      do: @ld_lib_paths |> Enum.join(":"),
      else: (@ld_lib_paths ++ [sys_ld_lib_path]) |> Enum.join(":")
  end
end
