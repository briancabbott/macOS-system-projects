class Jpegoptim < Formula
  desc "Utility to optimize JPEG files"
  homepage "https://github.com/tjko/jpegoptim"
  url "https://github.com/tjko/jpegoptim/archive/RELEASE.1.4.6.tar.gz"
  sha256 "c44dcfac0a113c3bec13d0fc60faf57a0f9a31f88473ccad33ecdf210b4c0c52"
  license "GPL-2.0"
  head "https://github.com/tjko/jpegoptim.git"

  bottle do
    cellar :any
    sha256 "ca4714cf1b1ecbc166a78b8143648fa639b70495f54dc75bcd1a71b9a2c4e604" => :big_sur
    sha256 "38149f489f36745c9be64ccaff694c53a07c6a8d4c98335703e7c1ba6206c5e0" => :arm64_big_sur
    sha256 "f6acdfbe5b3ff49f922bfccb936c39609bb1a0f9dbebd1289d1679bf7fe5b2a4" => :catalina
    sha256 "c60d59cfe20db5ad448c4da58d7c43ca072f15a31502b989a51b9020da445880" => :mojave
    sha256 "9588bffa63f2041939e480ff8dbce25a004ef2414fc7ea9d5b5177a38bfb8eaf" => :high_sierra
    sha256 "89b7f8465e95066c6bf19515affed14037841ea5d0a86b8c3d6cf026f507e938" => :sierra
    sha256 "cc6c60a27cba7bb5f0e1b4a7c8ae3567db4eeaf1e1384488b818da7a1409f837" => :el_capitan
  end

  depends_on "jpeg"

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    ENV.deparallelize # Install is not parallel-safe
    system "make", "install"
  end

  test do
    source = test_fixtures("test.jpg")
    assert_match "OK", shell_output("#{bin}/jpegoptim --noaction #{source}")
  end
end
