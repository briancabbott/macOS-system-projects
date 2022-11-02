class Vc4asm < Formula
  desc "Macro assembler for Broadcom VideoCore IV aka Raspberry Pi GPU"
  homepage "http://maazl.de/project/vc4asm/doc/index.html"
  url "https://github.com/maazl/vc4asm/archive/V0.2.3.tar.gz"
  sha256 "8d5f49f7573d1cc6a7baf7cee5e1833af2a87427ad8176989083c6ba7d034c8c"

  bottle do
    cellar :any_skip_relocation
    sha256 "f7899b26e3ad1fc2dd969b8178f24ddad5049164e30d6039443c9eac691b29a6" => :big_sur
    sha256 "86cda7631f6e50ef12f26198ec3810684b9d74599b12b1e68af2ae77857119ae" => :arm64_big_sur
    sha256 "d8a425ef7d84c5a1ba477c07e3b04f5fddb0dce92e5cf67a963ecfbc12b3caec" => :catalina
    sha256 "fc0a060875dd9233a3675b034055b1ae23d8775701529024b91f184a7e97521e" => :mojave
    sha256 "db9bbf5ee3cb47a0f3ffa1d9bf355205873237e9f2dbd26777546935401ef4b0" => :high_sierra
    sha256 "2547c982e3fde40316d01d802bd01bf49af208e6737ecafeaeb8ad988ea3255d" => :sierra
    sha256 "72d54a4237c4e0f952fd1a3d913725d84814ed5b657affa1d6dcafa19e1cdc44" => :el_capitan
    sha256 "871b3b109ac49b09056f83e4488105196060d2388dc5052c679776b43fab5927" => :yosemite
  end

  # Fixes "ar: illegal option combination for -r"
  # Reported 13 Apr 2017 https://github.com/maazl/vc4asm/issues/18
  resource "old_makefile" do
    url "https://raw.githubusercontent.com/maazl/vc4asm/c6991f0/src/Makefile"
    sha256 "2ea9a9e660e85dace2e9b1c9be17a57c8a91e89259d477f9f63820aee102a2d3"
  end

  def install
    ENV.cxx11

    # Fixes "error: use of undeclared identifier 'errno'"
    # Reported 13 Apr 2017 https://github.com/maazl/vc4asm/issues/19
    inreplace "src/utils.cpp", "#include <unistd.h>",
                               "#include <unistd.h>\n#include <errno.h>"

    (buildpath/"src").install resource("old_makefile")
    system "make", "-C", "src"
    bin.install "bin/vc4asm", "bin/vc4dis"
    share.install "share/vc4.qinc"
  end

  test do
    (testpath/"test.qasm").write <<~EOS
      mov -, sacq(9)
      add r0, r4, ra1.unpack8b
      add.unpack8ai r0, r4, ra1
      add r0, r4.8a, ra1
    EOS
    system "#{bin}/vc4asm", "-o test.hex", "-V", "#{share}/vc4.qinc", "test.qasm"
  end
end
