class Pachi < Formula
  desc "Software for the Board Game of Go/Weiqi/Baduk"
  homepage "https://pachi.or.cz/"
  url "https://github.com/pasky/pachi/archive/pachi-12.60.tar.gz"
  sha256 "3c05cf4fe5206ba4cbe0e0026ec3225232261b44e9e05e45f76193b4b31ff8e9"
  license "GPL-2.0"
  head "https://github.com/pasky/pachi.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "d14dec70d5fedd0d7ba63b05f175b06b12c40e1da71d24da64712ce63858dae1" => :big_sur
    sha256 "71f7bf11f6d68a8768468e4494cdc0785f484a5ccd7713cfc4327f049e79e80a" => :arm64_big_sur
    sha256 "9a2adc64bf7dbfbaf9e3d9ff940d6c5bcb0e4040160ed62f57751ec87281132e" => :catalina
    sha256 "c88f24dd1e7a267848eab540dc2b0961962825ab6e7088fc24b335159dacf31c" => :mojave
  end

  resource "patterns" do
    url "https://sainet-dist.s3.amazonaws.com/pachi_patterns.zip"
    sha256 "73045eed2a15c5cb54bcdb7e60b106729009fa0a809d388dfd80f26c07ca7cbc"
  end

  resource "book" do
    url "https://gnugo.baduk.org/books/ra6.zip"
    sha256 "1e7ffc75c424e94338308c048aacc479da6ac5cbe77c0df8adc733956872485a"
  end

  def install
    ENV["MAC"] = "1"
    ENV["DOUBLE_FLOATING"] = "1"

    # https://github.com/pasky/pachi/issues/78
    inreplace "Makefile", "build.h: .git/HEAD .git/index", "build.h:"
    inreplace "Makefile", "DCNN=1", "DCNN=0"

    system "make"
    bin.install "pachi"

    pkgshare.install resource("patterns")
    pkgshare.install resource("book")
  end

  def caveats
    <<~EOS
      This formula also downloads additional data, such as opening books
      and pattern files. They are stored in #{opt_pkgshare}.

      At present, pachi cannot be pointed to external files, so make sure
      to set the working directory to #{opt_pkgshare} if you want pachi
      to take advantage of these additional files.
    EOS
  end

  test do
    assert_match /^= [A-T][0-9]+$/, pipe_output("#{bin}/pachi", "genmove b\n", 0)
  end
end
