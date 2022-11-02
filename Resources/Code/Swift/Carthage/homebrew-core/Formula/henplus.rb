class Henplus < Formula
  desc "SQL shell that can handle multiple sessions in parallel"
  homepage "https://github.com/neurolabs/henplus"
  url "https://github.com/downloads/neurolabs/henplus/henplus-0.9.8.tar.gz"
  sha256 "ea7ca363d0503317235e97f66aa0efefe44463d8445e88b304ec0ac1748fe1ff"
  license "GPL-2.0"
  revision 1

  bottle do
    cellar :any_skip_relocation
    sha256 "a53828d49583605b23c96a1cbdcf50eabd946bcc77b4cb3b9beb02f620f7c372" => :catalina
    sha256 "4b11fa555ad538ba707fdfe601b105a7ed184be52f46d9aadb1d5ce7c53e2e15" => :mojave
    sha256 "87645aab44dcb857cf05f044d1991c7c06ee616b00acde904d3f470d2caf6ae3" => :high_sierra
    sha256 "2a66a4eeecd45406dc022d6f22a17aaf1ff3a22277296620a2771fb26a580afe" => :sierra
    sha256 "529052e2809a6c83143e68f2352defef79b72a72c91a27a16c5d0b778a51d729" => :el_capitan
    sha256 "8c3373e1459910f5c2df7c1849a881cf4caf71f75830f18b92bff9013b3178e8" => :yosemite
    sha256 "21f7ee166b94b30dd78cee8a6757fecd285544b8b12b332db9141e6a94eb29a7" => :mavericks
  end

  disable! date: "2020-12-08", because: :unmaintained

  depends_on "ant" => :build
  depends_on "libreadline-java"
  depends_on "openjdk@8"

  def install
    inreplace "bin/henplus" do |s|
      s.gsub! "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"
      s.change_make_var! "DYLD_LIBRARY_PATH", Formula["libreadline-java"].opt_lib
      s.gsub! "$THISDIR/..", HOMEBREW_PREFIX
      s.gsub! "share/java/libreadline-java.jar",
              "share/libreadline-java/libreadline-java.jar"
    end

    system "ant", "install", "-Dprefix=#{prefix}"
  end

  def caveats
    <<~EOS
      You may need to set JAVA_HOME:
        export JAVA_HOME="$(/usr/libexec/java_home)"
    EOS
  end

  test do
    system bin/"henplus", "--help"
  end
end
