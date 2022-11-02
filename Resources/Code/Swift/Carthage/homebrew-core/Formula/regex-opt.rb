class RegexOpt < Formula
  desc "Perl-compatible regular expression optimizer"
  homepage "https://bisqwit.iki.fi/source/regexopt.html"
  url "https://bisqwit.iki.fi/src/arch/regex-opt-1.2.4.tar.gz"
  sha256 "128c8ba9570b1fd8a6a660233de2f5a4022740bc5ee300300709c3894413883f"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "6c06ca3033c1a2d174368b4ca2194f732411c24911945cbad73b47a50ed1393b" => :big_sur
    sha256 "0d8b19c7d0c896626944d9affc850e42f0073ecbfe82b6380f0ed494c13bc759" => :arm64_big_sur
    sha256 "8a561d7a4dfadf25fd39bd5b19d6a8161a2f0d1be2c459cbe691be17aef85bc0" => :catalina
    sha256 "76b26dc9e766e7a8b0806660e966e3a49c593591b94d90439f89b7cbc797d019" => :mojave
    sha256 "0e46dec5d46b145e32ca597c00c75fea2e7097e57c5d3131be141e5bea2b96db" => :high_sierra
    sha256 "68b5f75c9fdb645334ae8a48a5b7e01620e19d5f103811579cb8bf96101c6ac7" => :sierra
  end

  def install
    system "make", "CC=#{ENV.cc}", "CXX=#{ENV.cxx}"
    bin.install "regex-opt"
  end

  test do
    output = shell_output("#{bin}/regex-opt foo...*..*bar")
    assert_equal "foo.{3,}bar", output
  end
end
