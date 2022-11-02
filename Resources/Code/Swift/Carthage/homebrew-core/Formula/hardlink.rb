class Hardlink < Formula
  desc "Replace file copies using hardlinks"
  homepage "https://jak-linux.org/projects/hardlink/"
  url "https://jak-linux.org/projects/hardlink/hardlink_0.3.0.tar.xz"
  sha256 "e8c93dfcb24aeb44a75281ed73757cb862cc63b225d565db1c270af9dbb7300f"

  bottle do
    cellar :any
    sha256 "1c2d9bd0578affd02e5b3ea25f09167665f555b652254cea27aabf1b704bf294" => :big_sur
    sha256 "fe5acfbc7a123db425beb0257ca23f4286b3260bd76b81027ee7528cc05bfdfd" => :arm64_big_sur
    sha256 "f0b2171598c5eb9111c2923649f46e32a182af7bc5e5f6012f4f13178651e3ed" => :catalina
    sha256 "971dab4459ef06afd11cf2cf7c0ade1ee7bcf959e359938f83b2b8a7d86a7d17" => :mojave
    sha256 "4738a658357798d756d8a96f96d3700f387ae89d1db769b81675634e85018c19" => :high_sierra
    sha256 "56ac75c51db6d7e19efe41eef24aa6646cdc126a113f5aacadd5f80043efc0d5" => :sierra
    sha256 "d8b6e2d26d8f49a207c5082a97f1e5c31b35041bcfbc17a217a1c2ad4ff68551" => :el_capitan
    sha256 "36c30ed90a3d2b9d2d4d07cb182c2838dfba276a05c22d022a42e16043e86f02" => :yosemite
    sha256 "cba1b82474c668bbb36e2e56cf7b36685924592d291dc05067d7c4a605686084" => :mavericks
  end

  depends_on "pkg-config" => :build
  depends_on "gnu-getopt"
  depends_on "pcre"

  on_linux do
    depends_on "attr"
  end

  def install
    system "make", "PREFIX=#{prefix}", "MANDIR=#{man}", "BINDIR=#{bin}", "install"
  end

  test do
    system "#{bin}/hardlink", "--help"
  end
end
