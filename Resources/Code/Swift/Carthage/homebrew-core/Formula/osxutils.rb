class Osxutils < Formula
  desc "Collection of macOS command-line utilities"
  homepage "https://github.com/specious/osxutils"
  url "https://github.com/specious/osxutils/archive/v1.9.0.tar.gz"
  sha256 "9c11d989358ed5895d9af7644b9295a17128b37f41619453026f67e99cb7ecab"
  license "GPL-2.0"
  head "https://github.com/specious/osxutils.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "499d88a58e5ab8ed2fc23e8ef3bd9234849a3d9df34b0c6bdae4b425be70d97b" => :big_sur
    sha256 "c5d4050cda7e5ede43231c7195ffa1eb06bf5e3b5a1efa6acf8243a0e8ee424a" => :arm64_big_sur
    sha256 "95f394fa7721dc587b75adcb0a698c32858bfabf04bb569b6bf6ab0d7f52fb03" => :catalina
    sha256 "744e327d1fb2183de8785880c3f7a127abdd896977e3d30cade00933ea137521" => :mojave
    sha256 "d665cbec1973b73e1e1d290014786b95d36d9cfe7028fd69fa37f698d18e81dd" => :high_sierra
    sha256 "8021183b4ad9c646920020e51446e555210bbb24e22da923557e1e0370353dfd" => :sierra
    sha256 "3bd65cf2550b709c111e31db7cb7d829a9260ed5dd35a682c370ed01593c1989" => :el_capitan
  end

  def install
    system "make"
    system "make", "PREFIX=#{prefix}", "install"
  end

  test do
    assert_match "osxutils", shell_output("#{bin}/osxutils")
  end
end
