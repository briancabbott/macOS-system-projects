class WirouterKeyrec < Formula
  desc "Recover the default WPA passphrases from supported routers"
  homepage "https://www.salvatorefresta.net/tools/"
  url "https://www.mirrorservice.org/sites/distfiles.macports.org/wirouterkeyrec/WiRouter_KeyRec_1.1.2.zip"
  mirror "https://distfiles.macports.org/wirouterkeyrec/WiRouter_KeyRec_1.1.2.zip"
  sha256 "3e59138f35502b32b47bd91fe18c0c232921c08d32525a2ae3c14daec09058d4"
  license "GPL-3.0"

  livecheck do
    url :homepage
    regex(%r{href=.*?/WiRouter_KeyRec[._-]v?(\d+(?:\.\d+)+)\.zip}i)
  end

  bottle do
    sha256 "f5a1ec8cb71d5240eb01a3dbd0cbfa8f09c4b76cae27cacd2fea058ccb8c9f78" => :big_sur
    sha256 "e3cfa2752a3957af0fcc474d4ad24ab76f026ee4479e0fa74d84222d16c02812" => :arm64_big_sur
    sha256 "907d4ed63f0f9c13217a9120749b12521ad773d310d554534a507ca9714d2dd7" => :catalina
    sha256 "ca8371cae9a6a4ce5ce4541a811d17260d877695426b16777e4b89d0fb912332" => :mojave
    sha256 "60a9b2a5fffe6027b296ad5b320377dd98a28658b628d6b3acbe94126e54ff3e" => :high_sierra
    sha256 "2accae4664406559e45909d53eaf6a8a8569773c8e0d932e2d3a8090706f8f18" => :sierra
    sha256 "3982522d8e15ced547c4f1d3019fe3ca6ddaa88d33fad03d2c97a53c849ec217" => :el_capitan
    sha256 "65d21ba4cb167dd2cca395dd5b51edc1ddd0df06fc65843cd2d2eba9e307ea11" => :yosemite
    sha256 "b5740e7929fc911881e007103921c712483971accb581bd5fdb86357f65b8312" => :mavericks
  end

  def install
    inreplace "src/agpf.h", %r{/etc}, "#{prefix}/etc"
    inreplace "src/teletu.h", %r{/etc}, "#{prefix}/etc"

    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}",
                          "--sysconfdir=#{prefix}",
                          "--exec-prefix=#{prefix}"
    system "make", "prefix=#{prefix}"
    system "make", "install", "DESTDIR=#{prefix}", "BIN_DIR=bin/"
  end

  test do
    system "#{bin}/wirouterkeyrec", "-s", "Alice-12345678"
  end
end
