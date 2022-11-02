class Espeak < Formula
  desc "Text to speech, software speech synthesizer"
  homepage "https://espeak.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/espeak/espeak/espeak-1.48/espeak-1.48.04-source.zip"
  sha256 "bf9a17673adffcc28ff7ea18764f06136547e97bbd9edf2ec612f09b207f0659"
  revision 1

  livecheck do
    url :stable
    regex(%r{url=.*?/espeak[._-]v?(\d+(?:\.\d+)+)(?:-source)?\.(?:t|zip)}i)
  end

  bottle do
    rebuild 1
    sha256 "c8d5f5fd950e7f47f48affb043ba950694c6480d7a12231eb1f2606ab4e05dbe" => :big_sur
    sha256 "0bd59ad014f2deeb623f5128f44e48a06f34106e3c46d228452595e44b6cdf17" => :arm64_big_sur
    sha256 "9e3a743f118a7ca9d177d005d260814d576fc9c72f5cad369204a8c42c54ffb4" => :catalina
    sha256 "055c918c267f825ed18f089c75db7c7408ea25ca93ba1a99e0aaba6f5b3a446d" => :mojave
    sha256 "ff4334be449510bdea51a7d853890fec167914658eb4c5167c5a6b40c6621ee2" => :high_sierra
    sha256 "ad40b912f2b0cf1b72ab89d53729cd61717a9d9b5bc588950cd6318b63c9e133" => :sierra
    sha256 "5e2829905c793de0ccf38ccca04e03bc504f7f70137952d44177461c16cbf174" => :el_capitan
    sha256 "7fed44fd08e3fbbc193e679d97141cf43facbd9a0661fb6a2991bebb5272864a" => :yosemite
    sha256 "4da1cfc5fe126fa8b0fd6b5909a10c9d6dee3536d772fa0d090f399134a5cd5b" => :mavericks
  end

  depends_on "portaudio"

  def install
    share.install "espeak-data"
    doc.install Dir["docs/*"]
    cd "src" do
      rm "portaudio.h"
      inreplace "Makefile", "SONAME_OPT=-Wl,-soname,", "SONAME_OPT=-Wl,-install_name,"
      # macOS does not use -soname so replacing with -install_name to compile for macOS.
      # See https://stackoverflow.com/questions/4580789/ld-unknown-option-soname-on-os-x/32280483#32280483
      inreplace "speech.h", "#define USE_ASYNC", "//#define USE_ASYNC"
      # macOS does not provide sem_timedwait() so disabling #define USE_ASYNC to compile for macOS.
      # See https://sourceforge.net/p/espeak/discussion/538922/thread/0d957467/#407d
      system "make", "speak", "DATADIR=#{share}/espeak-data", "PREFIX=#{prefix}"
      bin.install "speak" => "espeak"
      system "make", "libespeak.a", "DATADIR=#{share}/espeak-data", "PREFIX=#{prefix}"
      lib.install "libespeak.a"
      system "make", "libespeak.so", "DATADIR=#{share}/espeak-data", "PREFIX=#{prefix}"
      lib.install "libespeak.so.1.1.48" => "libespeak.dylib"
      MachO::Tools.change_dylib_id("#{lib}/libespeak.dylib", "#{lib}/libespeak.dylib")
      # macOS does not use the convention libraryname.so.X.Y.Z. macOS uses the convention libraryname.X.dylib
      # See https://stackoverflow.com/questions/4580789/ld-unknown-option-soname-on-os-x/32280483#32280483
    end
  end

  test do
    system "#{bin}/espeak", "This is a test for Espeak.", "-w", "out.wav"
  end
end
