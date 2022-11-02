class Netcat6 < Formula
  desc "Rewrite of netcat that supports IPv6, plus other improvements"
  homepage "https://www.deepspace6.net/projects/netcat6.html"
  url "https://deb.debian.org/debian/pool/main/n/nc6/nc6_1.0.orig.tar.gz"
  sha256 "db7462839dd135ff1215911157b666df8512df6f7343a075b2f9a2ef46fe5412"
  license "GPL-2.0"

  bottle do
    rebuild 1
    sha256 "746665ead519a4bbeb2984d7d83d8ea8425441f922bb199e0328da562e870144" => :big_sur
    sha256 "233f4e4769def17cb05da043221a3bb3460b984b570cd4ebbcce7342e7ab6c05" => :arm64_big_sur
    sha256 "fbba0de060d7d38efc84e3a098de48d127467fd8e6d90edf2ed96bc20b5e38df" => :catalina
    sha256 "aba098730e397f84b6ed7534b41bd7f65f5f6182189d890ac93216faff2fe9b7" => :mojave
    sha256 "b3fe44c42b33bc668cdaa0f05eb10a5f9b67891b1947b98abe9cad6464182835" => :high_sierra
    sha256 "bdb853a9a63a03555682eae734d9d9a7725591dfd16128cf59f208968ef16ef2" => :sierra
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    out = pipe_output("#{bin}/nc6 www.google.com 80", "GET / HTTP/1.0\r\n\r\n")
    assert_equal "HTTP/1.0 200 OK", out.lines.first.chomp
  end
end
