class Webarchiver < Formula
  desc "Allows you to create Safari .webarchive files"
  homepage "https://github.com/newzealandpaul/webarchiver"
  url "https://github.com/newzealandpaul/webarchiver/archive/0.10.tar.gz"
  sha256 "06705d361ebb1de6411e4d386a01a60314aa52d5c20f64603c1b9652a3eceae4"
  head "https://github.com/newzealandpaul/webarchiver.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "b25c37b0ebcc11c2e38b75f094e8772fbb5864fc5e32d67dd5d35e398f8a6ed4" => :big_sur
    sha256 "50bf552bad7141e95abb811a7bb8e200747183486e3875524281175c83e81941" => :arm64_big_sur
    sha256 "7413d45de93fbe6fcc3bc9af073a2bb063cdad7f3479d6bf890634d48761df22" => :catalina
    sha256 "ffd1e97727c1551d6bfc63ba3980469ca7be4c99bbd89c1036671a1e3463e3e3" => :mojave
    sha256 "fe85ee50f8a3da76dcbcd8bb24c1bea05bde33525055c4d471c8b07fccadfa65" => :high_sierra
  end

  depends_on xcode: ["6.0.1", :build]

  def install
    # Force 64 bit-only build, otherwise it fails on Mojave
    xcodebuild "SYMROOT=build", "-arch", "x86_64"

    bin.install "./build/Release/webarchiver"
  end

  test do
    system "#{bin}/webarchiver", "-url", "https://www.google.com", "-output", "foo.webarchive"
    assert_match /Apple binary property list/, shell_output("file foo.webarchive")
  end
end
