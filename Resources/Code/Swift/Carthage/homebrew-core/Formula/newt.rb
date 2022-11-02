class Newt < Formula
  desc "Library for color text mode, widget based user interfaces"
  homepage "https://pagure.io/newt"
  url "https://pagure.io/releases/newt/newt-0.52.21.tar.gz"
  sha256 "265eb46b55d7eaeb887fca7a1d51fe115658882dfe148164b6c49fccac5abb31"
  license "LGPL-2.0"

  livecheck do
    url "https://pagure.io/releases/newt/"
    regex(/href=.*?newt[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "93469c4ded76db63f8ac56143c9d110e18ff8c92f25857a7bb2955de63eb19cc" => :big_sur
    sha256 "5b125ba2ac186a9b1163e5546a3669d19dbd1c9703060a2ac358e4ca784ec2b9" => :arm64_big_sur
    sha256 "82ea49582f5bcf3bbaf6a39d4d6128c966889eaff682bf83601954c995ee1276" => :catalina
    sha256 "2b902ecc6fc52b2f2681eb23e4eb568684a018deeada88e5a920952de8cc1080" => :mojave
    sha256 "7a88cbe033eb207df57a6410f19339975a672e331c073e3ee79e467652c8753c" => :high_sierra
    sha256 "4d82531bb783ba5e3f8a64150582e61b2fd4a9fb8be96d0bee88fa3bbe0dc3c3" => :sierra
  end

  depends_on "gettext"
  depends_on "popt"
  depends_on "s-lang"

  def install
    args = ["--prefix=#{prefix}", "--without-tcl"]

    inreplace "Makefile.in" do |s|
      # name libraries correctly
      # https://bugzilla.redhat.com/show_bug.cgi?id=1192285
      s.gsub! "libnewt.$(SOEXT).$(SONAME)", "libnewt.$(SONAME).dylib"
      s.gsub! "libnewt.$(SOEXT).$(VERSION)", "libnewt.$(VERSION).dylib"

      # don't link to libpython.dylib
      # causes https://github.com/Homebrew/homebrew/issues/30252
      # https://bugzilla.redhat.com/show_bug.cgi?id=1192286
      s.gsub! "`$$pyconfig --ldflags`", '"-undefined dynamic_lookup"'
      s.gsub! "`$$pyconfig --libs`", '""'
    end

    system "./configure", *args
    system "make", "install"
  end

  test do
    ENV["TERM"] = "xterm"
    (testpath/"test.c").write <<~EOS
      #import <newt.h>
      int main() {
        newtInit();
        newtFinished();
      }
    EOS
    system ENV.cc, "test.c", "-o", "test", "-L#{lib}", "-lnewt"
    system "./test"
  end
end
