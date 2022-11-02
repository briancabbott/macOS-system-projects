class Snownews < Formula
  desc "Text mode RSS newsreader"
  homepage "https://github.com/kouya/snownews"
  url "https://github.com/kouya/snownews/archive/1.6.10.tar.gz"
  sha256 "8c78067aef75e283df4b3cca1c966587b6654e9e84a3e6e5eb8bdd5829799242"
  license "GPL-3.0"

  bottle do
    sha256 "4b9252e14c89712ad72b1c4e52d467ef36ec0f8a53b15083f11120dade87f7a0" => :big_sur
    sha256 "8f6543fe8c6e40ec9e04b38178d1945fc3f80141f9c1035235232771807c04de" => :arm64_big_sur
    sha256 "6829a661dc7dbd01b149a05aff6b81b4ecc22b99f3f06345f029b183bf952c64" => :catalina
    sha256 "db1d20c1468867a2eeb77b521fdfac4e167393797506afd6bbd603fb40c435bf" => :mojave
    sha256 "0e1eed435b2c94a95e35ee89c3b68be6cfa9867d75dc9fbb46b19a66ff6da915" => :high_sierra
    sha256 "6437a67fb63f92f3c4d57b69505cb5146e2d7325da0d6fd73b57ac0f1461c807" => :sierra
  end

  depends_on "coreutils" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "ncurses"

  uses_from_macos "libxml2"

  def install
    # Fix file not found errors for /usr/lib/system/libsystem_symptoms.dylib and
    # /usr/lib/system/libsystem_darwin.dylib on 10.11 and 10.12, respectively
    ENV["SDKROOT"] = MacOS.sdk_path if MacOS.version <= :sierra

    system "./configure", "--prefix=#{prefix}"

    # Must supply -lz because configure relies on "xml2-config --libs"
    # for it, which doesn't work on OS X prior to 10.11
    system "make", "install", "EXTRA_LDFLAGS=#{ENV.ldflags} -L#{Formula["openssl@1.1"].opt_lib} -lz",
           "CC=#{ENV.cc}", "INSTALL=ginstall"
  end

  test do
    system bin/"snownews -V"
  end
end
