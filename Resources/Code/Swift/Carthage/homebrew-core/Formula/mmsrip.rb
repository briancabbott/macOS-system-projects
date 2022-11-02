class Mmsrip < Formula
  desc "Client for the MMS:// protocol"
  homepage "https://nbenoit.tuxfamily.org/index.php?page=MMSRIP"
  url "https://nbenoit.tuxfamily.org/projects/mmsrip/mmsrip-0.7.0.tar.gz"
  sha256 "5aed3cf17bfe50e2628561b46e12aec3644cfbbb242d738078e8b8fce6c23ed6"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "74c94f8562cc8c71a8376fc3a294a05a78c2a520ee7cb38a4996577d8417a06f" => :big_sur
    sha256 "e5d47cddeabd5f3cbd7b0c2c988d10dee8726dcf557f95eb3cada3a1cdc954a7" => :arm64_big_sur
    sha256 "084dec614496303468f92768c1f262f3a72abf9b839791e84711ed9288efb402" => :catalina
    sha256 "1cff3346265ccfa27553e90225de862c7a1ac61ff54c8fdb8fabfc4204d1bad3" => :mojave
    sha256 "e7c12a6c8e6ef612d1c789fad3e06c0b21acfe6e4dbac1643ae7797faeafcb35" => :high_sierra
    sha256 "b4578327661828737b3aa71615806ba6e2781d7c0815a12815023242ac80e598" => :sierra
    sha256 "cf0bc6b407f4861b174eddf55ae5da45330d37abc428013ca19f173d36a96d2a" => :el_capitan
    sha256 "07f4c9d5a84bb52e3c799ebe7c395a4939c0c7ac5dff0fb46e2ce84abd9b5417" => :yosemite
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}", "--mandir=#{man}"
    system "make"
    system "make", "install"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/mmsrip --version 2>&1")
  end
end
