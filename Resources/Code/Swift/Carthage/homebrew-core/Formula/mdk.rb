class Mdk < Formula
  desc "GNU MIX development kit"
  homepage "https://www.gnu.org/software/mdk/mdk.html"
  url "https://ftp.gnu.org/gnu/mdk/v1.3.0/mdk-1.3.0.tar.gz"
  mirror "https://ftpmirror.gnu.org/mdk/v1.3.0/mdk-1.3.0.tar.gz"
  sha256 "8b1e5dd7f47b738cb966ef717be92a501494d9ba6d87038f09e8fa29101b132e"
  license "GPL-3.0-or-later"

  livecheck do
    url :stable
    regex(%r{href=["']?v?(\d+(?:\.\d+)+)/?["' >]}i)
  end

  bottle do
    sha256 "305422ac29e1cb04827277976b3c2e7fe678a00cd2a648739d00684c9c1f3a78" => :big_sur
    sha256 "bd29f7cd3b52987492d17a4cfa9a51712bbacda1f738454cfb942596392fe9f7" => :catalina
    sha256 "51c33dc12bf9277cd0d60d55a34236a1ab8d9577c9fbe296a8d893962e391d6a" => :mojave
    sha256 "e8bd4f2623b6e6e55cc2ccf30339a39f14cc1b499d155b6c33144fdf0bf76745" => :high_sierra
  end

  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "adwaita-icon-theme"
  depends_on "flex"
  depends_on "glib"
  depends_on "gtk+3"
  depends_on "guile"
  depends_on "readline"

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    ENV["LANG"] = "en_US.UTF-8"

    (testpath/"hello.mixal").write <<~EOS
      *                                                        (1)
      * hello.mixal: say "hello world" in MIXAL                (2)
      *                                                        (3)
      * label ins    operand     comment                       (4)
      TERM    EQU    19          the MIX console device number (5)
              ORIG   1000        start address                 (6)
      START   OUT    MSG(TERM)   output data at address MSG    (7)
              HLT                halt execution                (8)
      MSG     ALF    "MIXAL"                                   (9)
              ALF    " HELL"                                   (10)
              ALF    "O WOR"                                   (11)
              ALF    "LD"                                      (12)
              END    START       end of the program            (13)
    EOS
    system "#{bin}/mixasm", "hello"
    output = `#{bin}/mixvm -r hello`

    expected = <<~EOS
      Program loaded. Start address: 1000
      Running ...
      MIXAL HELLO WORLDXXX
      ... done
    EOS
    expected = expected.gsub("XXX", " " *53)

    assert_equal expected, output
  end
end
