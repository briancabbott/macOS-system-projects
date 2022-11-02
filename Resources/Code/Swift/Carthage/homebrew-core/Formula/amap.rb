class Amap < Formula
  desc "Perform application protocol detection"
  homepage "https://github.com/vanhauser-thc/THC-Archive"
  url "https://github.com/vanhauser-thc/THC-Archive/raw/master/Tools/amap-5.4.tar.gz"
  mirror "https://downloads.sourceforge.net/project/slackbuildsdirectlinks/amap/amap-5.4.tar.gz"
  sha256 "a75ea58de75034de6b10b0de0065ec88e32f9e9af11c7d69edbffc4da9a5b059"
  revision 3

  livecheck do
    url "https://github.com/vanhauser-thc/THC-Archive/tree/master/Tools/"
    strategy :page_match
    regex(%r{href=.*?/amap[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "48480e1e415d4003efdfac48d4f5ae75c2dbfa1d53d9e742ca400cffa04dc231" => :catalina
    sha256 "ede3ed735b1041b7bb99595ffdbb49e392dbb84065383e32c0e270f8bea22da4" => :mojave
    sha256 "6266dd3178e2ed39f7a48e6c9fc19fbb073f4e7d71686d5ef3ce0ee660ccb982" => :high_sierra
    sha256 "1361e89caf2590146c5872907f90ad67ac9b99d2198320691e9f6df0cfdbe16c" => :sierra
  end

  disable! date: "2020-11-12", because: :unmaintained

  depends_on "openssl@1.1"

  def install
    # Last release was 2011 & there's nowhere supported to report this.
    openssl = Formula["openssl@1.1"]
    inreplace "configure" do |s|
      s.gsub! 'SSL_IPATH=""', "SSL_IPATH=\"#{openssl.opt_include}/openssl\""
      s.gsub! 'SSL_PATH=""', "SSL_PATH=\"#{openssl.opt_lib}\""
      s.gsub! 'CRYPTO_PATH=""', "CRYPTO_PATH=\"#{openssl.opt_lib}\""
    end

    system "./configure", "--prefix=#{prefix}"
    system "make"

    # --prefix doesn't work as we want it to so install manually
    bin.install "amap", "amap6", "amapcrap"
    etc.install "appdefs.resp", "appdefs.rpc", "appdefs.trig"
    man1.install "amap.1"
  end

  test do
    openssl_linked = MachO::Tools.dylibs("#{bin}/amap").any? { |d| d.include? Formula["openssl@1.1"].opt_lib.to_s }
    assert openssl_linked
    # We can do more than this, but unsure how polite it is to port-scan
    # someone's domain every time this goes through CI.
    assert_match version.to_s, shell_output("#{bin}/amap", 255)
  end
end
