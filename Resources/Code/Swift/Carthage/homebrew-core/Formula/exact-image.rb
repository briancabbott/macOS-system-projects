class ExactImage < Formula
  desc "Image processing library"
  homepage "https://exactcode.com/opensource/exactimage/"
  url "https://dl.exactcode.de/oss/exact-image/exact-image-1.0.2.tar.bz2"
  sha256 "0694c66be5dec41377acead475de69b3d7ffb42c702402f8b713f8b44cdc2791"
  license "GPL-2.0"

  livecheck do
    url "https://dl.exactcode.de/oss/exact-image/"
    regex(/href=.*?exact-image[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "9b3619df825bd01981c7a7b6fd1b6f88346d7d0fbbb7f9ed8fc30f9fef41cab0" => :big_sur
    sha256 "47aa8c7861a759d66f553bc8960ba09c14a3b5acf86e2c0f22779379716cac5f" => :arm64_big_sur
    sha256 "78a802b0edd2c27640aa2e6be381c146a7fa05bd6dd584ace90b1dfa0e426291" => :catalina
    sha256 "942bfd38bf5fd52613c936077eee5d5f71530325c7337e9db84e44e0b6c643a0" => :mojave
    sha256 "b182c3fa086d336ee9e6688bb341ea3df8ace70cac451fb757e88ba15c925365" => :high_sierra
    sha256 "1a9fc0dbba69ee471deabc6759ca52f3d669a535e021ef2defa33321261010ca" => :sierra
  end

  depends_on "pkg-config" => :build
  depends_on "libagg"

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/bardecode"
  end
end
