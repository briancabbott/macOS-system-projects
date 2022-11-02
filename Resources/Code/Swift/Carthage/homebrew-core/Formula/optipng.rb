class Optipng < Formula
  desc "PNG file optimizer"
  homepage "https://optipng.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/optipng/OptiPNG/optipng-0.7.7/optipng-0.7.7.tar.gz"
  sha256 "4f32f233cef870b3f95d3ad6428bfe4224ef34908f1b42b0badf858216654452"
  license "Zlib"
  head "http://hg.code.sf.net/p/optipng/mercurial", using: :hg

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "5cee26efb92016f057a55b2711a08c4a0350046b7c0b1d969c75a913caf66fc2" => :big_sur
    sha256 "796af028b1dea8b680e40103712976b4f9df285df553db06d2643779630c716c" => :arm64_big_sur
    sha256 "3d423dfa59e07122f70e2a15026289dfc6884798ac76898065dbe587256c6e35" => :catalina
    sha256 "bd44fa66b00a6ee0340a9a5b239d22823787fcaa26312783b21c0f4afc39fd0b" => :mojave
  end

  depends_on "libpng"

  uses_from_macos "zlib"

  def install
    system "./configure", "--with-system-zlib",
                          "--with-system-libpng",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    system "#{bin}/optipng", "-simulate", test_fixtures("test.png")
  end
end
