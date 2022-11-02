class GnomeThemesStandard < Formula
  desc "Default themes for the GNOME desktop environment"
  homepage "https://gitlab.gnome.org/GNOME/gnome-themes-extra"
  url "https://download.gnome.org/sources/gnome-themes-standard/3.22/gnome-themes-standard-3.22.3.tar.xz"
  sha256 "61dc87c52261cfd5b94d65e8ffd923ddeb5d3944562f84942eeeb197ab8ab56a"
  revision 2

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "99b5b918d997d6fee0c44ceb91845b4b1d247709ba01fcd917802aa6824d0e97" => :big_sur
    sha256 "e7f20854195f04389a9ebbd968b59541979fbea64379a45af6c3b09f850a5f6e" => :arm64_big_sur
    sha256 "6fb1066c6af0428fee29272851b4d7fbf10bac3bec4ed48ce6cffb780a3175f1" => :catalina
    sha256 "0275e08061a7fc1c641729075add70362499309548d9f82a65f30397fe756073" => :mojave
    sha256 "7c871fcd54d59a07719e5b1f22ca003921e479548ee9d13c5910af482b47891e" => :high_sierra
    sha256 "7e5bfe5894c0498b6b9325a782e4ea1c756b042d527815547cba6e6f411095a2" => :sierra
  end

  depends_on "gettext" => :build
  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "gtk+"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}",
                          "--disable-gtk3-engine"

    system "make", "install"
  end

  test do
    assert_predicate share/"icons/HighContrast/scalable/actions/document-open-recent.svg", :exist?
    assert_predicate lib/"gtk-2.0/2.10.0/engines/libadwaita.so", :exist?
  end
end
