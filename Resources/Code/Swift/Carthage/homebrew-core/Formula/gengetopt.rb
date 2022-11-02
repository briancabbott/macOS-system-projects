class Gengetopt < Formula
  desc "Generate C code to parse command-line arguments via getopt_long"
  homepage "https://www.gnu.org/software/gengetopt/"
  url "https://ftp.gnu.org/gnu/gengetopt/gengetopt-2.23.tar.xz"
  mirror "https://ftpmirror.gnu.org/gengetopt/gengetopt-2.23.tar.xz"
  sha256 "b941aec9011864978dd7fdeb052b1943535824169d2aa2b0e7eae9ab807584ac"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "f57ffafddb271d729ec0c07bdc564fc1bf0a52ee9b060cde1c5c8da3dbe3f515" => :big_sur
    sha256 "0d869a2ffa87824b4e5854df92e2f60f652fe4e5e20d188207bf6171d7d18f84" => :arm64_big_sur
    sha256 "7134042a80bb314db08216b1de2d293b5925ab729ba87649fdab4dc6298256f4" => :catalina
    sha256 "2ae5eeef439a6abc4d1f65965e1bafa9ac5ad0620cb4ef5e9444a4b2dbef1872" => :mojave
    sha256 "00f2578e7697c01d060a422e1be0ce8f4c6d23b365967ff7b5501d5cd6306dd1" => :high_sierra
    sha256 "57acd0ca20988a1b4f0f16383edb985549597b8a5266316e3a314b7775bab3c0" => :sierra
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"

    ENV.deparallelize
    system "make", "install"
  end

  test do
    ggo = <<~EOS
      package "homebrew"
      version "0.9.5"
      purpose "The missing package manager for macOS"

      option "verbose" v "be verbose"
    EOS

    pipe_output("#{bin}/gengetopt --file-name=test", ggo, 0)
    assert_predicate testpath/"test.h", :exist?
    assert_predicate testpath/"test.c", :exist?
    assert_match(/verbose_given/, File.read("test.h"))
  end
end
