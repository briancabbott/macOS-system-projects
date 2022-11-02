class Glkterm < Formula
  desc "Terminal-window Glk library"
  homepage "https://www.eblong.com/zarf/glk/"
  url "https://www.eblong.com/zarf/glk/glkterm-104.tar.gz"
  version "1.0.4"
  sha256 "473d6ef74defdacade2ef0c3f26644383e8f73b4f1b348e37a9bb669a94d927e"

  bottle do
    cellar :any_skip_relocation
    sha256 "a82e9471f88cd16b842beb87305959fcdec9fbc083cb7e4b6b213cb7f7c9f701" => :big_sur
    sha256 "dfa6c028a6b6c70b258e19faa4f274c5c993ee55d8fa21e7574aa1df32e6cd2c" => :arm64_big_sur
    sha256 "c337df9d5b7c6343fe21abf1f17143d51d4e61e747b1c6da7d31ad557653a7a0" => :catalina
    sha256 "34bba71e2063d751f179adf09caa65b6815b94b0f5c64436f20f3117e038e128" => :mojave
    sha256 "1e7d75d921b11cd91354b2f8acf8a63416709b7875146d095bcf1ce02cc6fdad" => :high_sierra
    sha256 "b4c65e282b8cf6fce1e32e4e168aef241d6c38f2090448c68ad3ca7157e1d473" => :sierra
    sha256 "b9db7677c23716a7f8a57ce45d309487a36cc41c1388e2c7990b49c17e2f0bb7" => :el_capitan
    sha256 "61b75bf1232fb3aacc5966ea13e88fe339da7ffd7c9882bab549456dd816a30a" => :yosemite
  end

  keg_only "conflicts with other Glk libraries"

  def install
    system "make"

    lib.install "libglkterm.a"
    include.install "glk.h", "glkstart.h", "gi_blorb.h", "gi_dispa.h", "Make.glkterm"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include "glk.h"
      #include "glkstart.h"

      glkunix_argumentlist_t glkunix_arguments[] = {
          { NULL, glkunix_arg_End, NULL }
      };

      int glkunix_startup_code(glkunix_startup_t *data)
      {
          return TRUE;
      }

      void glk_main()
      {
          glk_exit();
      }
    EOS
    system ENV.cc, "test.c", "-I#{include}", "-L#{lib}", "-lglkterm", "-lncurses", "-o", "test"
    system "echo test | ./test"
  end
end
