defmodule ExQuic.BundlexProject do
  use Bundlex.Project

  def project do
    [
      natives: natives()
    ]
  end

  defp natives() do
    [
      client: [
        sources: ["client.c", "lsquic_utils.c"],
        deps: [unifex: :unifex],
        compiler_flags: ["-D_POSIX_SOURCE=1"],
        pkg_configs: ["zlib"],
        libs: ["lsquic", "ssl", "crypto"],
        lib_dirs: [
          Path.expand("./third_party/lsquic/src/liblsquic"),
          Path.expand("./third_party/boringssl/ssl"),
          Path.expand("./third_party/boringssl/crypto")
        ],
        includes: ["third_party/lsquic/include",
          "third_party/boringssl/include",
          "c_src/ex_quic/libev"],
        interface: :cnode,
        preprocessor: Unifex
      ]
    ]
  end
end
