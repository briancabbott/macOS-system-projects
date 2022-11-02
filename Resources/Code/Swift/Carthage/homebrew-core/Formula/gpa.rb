class Gpa < Formula
  desc "Graphical user interface for the GnuPG"
  homepage "https://www.gnupg.org/related_software/gpa/"
  url "https://gnupg.org/ftp/gcrypt/gpa/gpa-0.10.0.tar.bz2"
  mirror "https://deb.debian.org/debian/pool/main/g/gpa/gpa_0.10.0.orig.tar.bz2"
  sha256 "95dbabe75fa5c8dc47e3acf2df7a51cee096051e5a842b4c9b6d61e40a6177b1"
  revision 2

  livecheck do
    url "https://gnupg.org/ftp/gcrypt/gpa/"
    regex(/href=.*?gpa[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "b4476f76bdd1e9b7acb836a49cb7e216726d277d04d24ce1b5c2d3d7e392adf0" => :big_sur
    sha256 "77f73e7161f535ed00a02c8df2f5818dad8574a0ff52f339949b1030a0bd7454" => :arm64_big_sur
    sha256 "c598e546d83f042d3de1011bff926a839c34e56c06a4c9cc6dbab25ff9c19df6" => :catalina
    sha256 "14eace8606e49fe9d3d2fa39a9f79fbbaca7cff7d78c0cb7027033f92133fa04" => :mojave
  end

  depends_on "pkg-config" => :build
  depends_on "desktop-file-utils"
  depends_on "gpgme"
  depends_on "gtk+"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end

  test do
    system "#{bin}/gpa", "--version"
  end
end
