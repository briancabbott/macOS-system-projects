class Chicken < Formula
  desc "Compiler for the Scheme programming language"
  homepage "https://www.call-cc.org/"
  url "https://code.call-cc.org/releases/5.2.0/chicken-5.2.0.tar.gz"
  sha256 "819149c8ce7303a9b381d3fdc1d5765c5f9ac4dee6f627d1652f47966a8780fa"
  head "https://code.call-cc.org/git/chicken-core.git"

  livecheck do
    url "https://code.call-cc.org/releases/current/"
    regex(/href=.*?chicken[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "1d723ed0cb6621708f2123882a05fffa9328f1ebdedb505f60746e5a1740761d" => :big_sur
    sha256 "8245210e28c0dd3ee7605efed72f157e49751e67e4c9eea279e5fc558a413278" => :arm64_big_sur
    sha256 "674b9d864481f15a5b406c1ef2e1dfce8ee584a100edf2501a096afee44ad396" => :catalina
    sha256 "3d35a95b8296a8e37c5bbaf5d77188684adcccc7f3f3d77e73c6c3e9ac566f86" => :mojave
    sha256 "17b093038bb0845a2687c1294288a11992f4e2279a64c93ef0e2c80977a1d882" => :high_sierra
  end

  def install
    ENV.deparallelize

    args = %W[
      PLATFORM=macosx
      PREFIX=#{prefix}
      C_COMPILER=#{ENV.cc}
      LIBRARIAN=ar
      POSTINSTALL_PROGRAM=install_name_tool
      ARCH=x86-64
    ]

    system "make", *args
    system "make", "install", *args
  end

  test do
    assert_equal "25", shell_output("#{bin}/csi -e '(print (* 5 5))'").strip
  end
end
