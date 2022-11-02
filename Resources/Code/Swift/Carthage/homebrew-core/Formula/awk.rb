class Awk < Formula
  desc "Text processing scripting language"
  homepage "https://www.cs.princeton.edu/~bwk/btl.mirror/"
  url "https://github.com/onetrueawk/awk/archive/20180827.tar.gz"
  sha256 "c9232d23410c715234d0c26131a43ae6087462e999a61f038f1790598ce4807f"
  # https://fedoraproject.org/wiki/Licensing:MIT?rd=Licensing/MIT#Standard_ML_of_New_Jersey_Variant
  license "MIT"
  head "https://github.com/onetrueawk/awk.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "561c3c6eafa4b668c0fa255ef85b764895f48ac5e2c36557592f551d065ef251" => :big_sur
    sha256 "2324b210fc998b6565cbf70feb072d959b3ffcd7e2087a3bc931db9fe551aafd" => :arm64_big_sur
    sha256 "2920fef8c3a7f5c3e45480b002968a860b0fbe36408cd0c0f1edb94a9b3c67b5" => :catalina
    sha256 "da17e7e893d2a2fb4ab267fb9ead8785ef9417dead77d6c84204d2151330bf47" => :mojave
    sha256 "3e7c18b44cd1f1783a28c34edbc2215a2b975021ec42ccaa0f792243d3cb320b" => :high_sierra
    sha256 "2c55499ad7ed357a30d643430dd00d426fd3cfa2f5705c772f5a3dd8c8cd020c" => :sierra
    sha256 "a844637c334c68f7d7079a1ef6bc45c4df242c93cf6ed891b6d551269518c9c7" => :el_capitan
  end

  conflicts_with "gawk",
    because: "both install an `awk` executable"

  def install
    ENV.O3 # Docs recommend higher optimization
    ENV.deparallelize
    # the yacc command the makefile uses results in build failures:
    # /usr/bin/bison: missing operand after `awkgram.y'
    # makefile believes -S to use sprintf instead of sprint, but the
    # -S flag is not supported by `bison -y`
    system "make", "CC=#{ENV.cc}", "CFLAGS=#{ENV.cflags}", "YACC=yacc -d"
    bin.install "a.out" => "awk"
    man1.install "awk.1"
  end

  test do
    assert_match "test", pipe_output("#{bin}/awk '{print $1}'", "test")
  end
end
