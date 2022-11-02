class Cpulimit < Formula
  desc "CPU usage limiter"
  homepage "https://github.com/opsengine/cpulimit"
  url "https://github.com/opsengine/cpulimit/archive/v0.2.tar.gz"
  sha256 "64312f9ac569ddcadb615593cd002c94b76e93a0d4625d3ce1abb49e08e2c2da"
  license "GPL-2.0"
  head "https://github.com/opsengine/cpulimit.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "f3f394e17febb7af49a1cb35c46e33856263dc58016d959aad2d2a250aae1d7d" => :big_sur
    sha256 "f09919436a14d7b1598720ca832435b2500aebe0839f5055a253f52c59642a5d" => :arm64_big_sur
    sha256 "7330907348c0a181c75b069dba7ee628e8c524c9bb9510487dbfd43730173db5" => :catalina
    sha256 "b9c7f99cbc62eb7c02b19c63a9b7e3f9186175707ff853a7107447fd7b2ee249" => :mojave
    sha256 "077ab8835a3b44ce77e3b8bf867633115b1d056046b232e49aeac96ac30e731c" => :high_sierra
    sha256 "fa5bc8d713837693c6bbd6139bec5e48b8a1d46ef669b2e042715dd1318b1655" => :sierra
    sha256 "9d7320465152a12ba75ce924beada5a3ce365b14becaa75e08ee8334c2cb2f6a" => :el_capitan
    sha256 "7ff9d929c5a1178b250b756cefcbecc4b202c72f03073e9eb43f4a47420930a8" => :yosemite
    sha256 "c24a495cd69c62693bcb0bd2a44c41c5bca84f0b9754019681816c1d2b47fe3e" => :mavericks
  end

  def install
    system "make"
    bin.install "src/cpulimit"
  end

  test do
    system "#{bin}/cpulimit", "--limit=10", "ls"
  end
end
