class Unrtf < Formula
  desc "RTF to other formats converter"
  homepage "https://www.gnu.org/software/unrtf/"
  url "https://ftp.gnu.org/gnu/unrtf/unrtf-0.21.10.tar.gz"
  mirror "https://ftpmirror.gnu.org/unrtf/unrtf-0.21.10.tar.gz"
  sha256 "b49f20211fa69fff97d42d6e782a62d7e2da670b064951f14bbff968c93734ae"
  license "GPL-3.0"
  head "https://hg.savannah.gnu.org/hgweb/unrtf/", using: :hg

  livecheck do
    url :stable
  end

  bottle do
    sha256 "198691cb483c4ae73b4c676d289bee8040937afe2881e07afbfb7b9f1e99a760" => :big_sur
    sha256 "7a091ab8d8e5a67f2821d1436300d6c41c9f15ead01a83ade9d38fc9cc2494b6" => :arm64_big_sur
    sha256 "90361817069fa7149b201a0caf5e65abd872d10f8fdda154ff450511debf1d99" => :catalina
    sha256 "b038c53ba7341cc9365db6cf9d46c6f7c3feba843643168e24a12856a29a6dbb" => :mojave
    sha256 "9abc63bdeae500637c8e1d6d31c72be013d0f2cf8ad8e3f1cb6e3babe5b6d94a" => :high_sierra
    sha256 "4c9e869dad1a76bf4077d9e19cabf9d383ed914b5a1c348dadc1eb0961c23b0a" => :sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build

  def install
    system "./bootstrap"
    system "./configure", "LIBS=-liconv", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.rtf").write <<~'EOS'
      {\rtf1\ansi
      {\b hello} world
      }
    EOS
    expected = <<~EOS
      <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
      <html>
      <head>
      <meta http-equiv="content-type" content="text/html; charset=utf-8">
      <!-- Translation from RTF performed by UnRTF, version #{version} -->
      </head>
      <body><b>hello</b> world</body>
      </html>
    EOS
    assert_equal expected, shell_output("#{bin}/unrtf --html test.rtf")
  end
end
