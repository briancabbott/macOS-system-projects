class CmuSphinxbase < Formula
  desc "Lightweight speech recognition engine for mobile devices"
  homepage "https://cmusphinx.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/cmusphinx/sphinxbase/0.8/sphinxbase-0.8.tar.gz"
  sha256 "55708944872bab1015b8ae07b379bf463764f469163a8fd114cbb16c5e486ca8"
  license "BSD-2-Clause"

  livecheck do
    url :stable
    regex(%r{url=.*?/sphinxbase[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "774670d79572ac633b5acf646a15ca54055e140044476bfeac7bc1d377a1dbe4" => :big_sur
    sha256 "d8ca2666d2ee6c5ff4a5a88ad086cfcf3e0cf744e6614ea31b451dfd86196c17" => :arm64_big_sur
    sha256 "0cea9513b180773ff3c45d24453e962ef4ad5d1f923c4c22716437f3580b195f" => :catalina
    sha256 "b55c9f16e8b89fc515d9bf8bd6ed91f532d0c82a46be01cd9792bb27076a6a51" => :mojave
    sha256 "2ebde8d649a3e78c3e219c83e1f12e6cee924f5404b0d68e8fe7d220c8dad0f5" => :high_sierra
    sha256 "fde603304716876e192bef822f8df21c26e09688d43580d3f9a61c78e03dbbb0" => :sierra
  end

  head do
    url "https://github.com/cmusphinx/sphinxbase.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
    depends_on "swig" => :build
  end

  depends_on "pkg-config" => :build
  # If these are found, they will be linked against and there is no configure
  # switch to turn them off.
  depends_on "libsamplerate"
  depends_on "libsndfile"

  def install
    if build.head?
      ENV["NOCONFIGURE"] = "yes"
      system "./autogen.sh"
    end
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include "cmd_ln.h"

      int main(int argc, char **argv) {
        cmd_ln_t *config = NULL;

        config = cmd_ln_init(NULL, NULL, TRUE,
          "-hello", "world", NULL);
        cmd_ln_free_r(config);
        return 0;
      }
    EOS
    system ENV.cxx, "-L#{lib}", "-lsphinxbase", "-I#{include}/sphinxbase", "test.cpp", "-o", "test"
    system "./test"
  end
end
