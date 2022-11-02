class Sylpheed < Formula
  desc "Simple, lightweight email-client"
  homepage "https://sylpheed.sraoss.jp/en/"
  url "https://sylpheed.sraoss.jp/sylpheed/v3.7/sylpheed-3.7.0.tar.bz2"
  sha256 "eb23e6bda2c02095dfb0130668cf7c75d1f256904e3a7337815b4da5cb72eb04"
  revision 4

  livecheck do
    url "https://sylpheed.sraoss.jp/en/download.html"
    regex(%r{stable.*?href=.*?/sylpheed[._-]v?(\d+(?:\.\d+)+)\.t}im)
  end

  bottle do
    sha256 "b8d825cf9222f047cf9eec78a8a8b81c8133cd75ded1c66e3423d38318226c41" => :big_sur
    sha256 "ae2f9828834200f3a587a7d596560bf8809f173a1044e997a58a0ea8d8a45acc" => :arm64_big_sur
    sha256 "294ac17fa03002cb92f7f1bcb5f1a9b4f56157e54b564bd8e4e673f5902fc8a0" => :catalina
    sha256 "80a9483de9580d154fe32831a5172cc5e72b31a3722f8335e39aa5fd763935ff" => :mojave
  end

  depends_on "pkg-config" => :build
  depends_on "gpgme"
  depends_on "gtk+"
  depends_on "openssl@1.1"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-updatecheck"
    system "make", "install"
  end

  test do
    system "#{bin}/sylpheed", "--version"
  end
end
