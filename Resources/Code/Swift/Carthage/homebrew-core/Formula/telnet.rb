class Telnet < Formula
  desc "User interface to the TELNET protocol"
  homepage "https://opensource.apple.com/"
  url "https://opensource.apple.com/tarballs/remote_cmds/remote_cmds-63.tar.gz"
  sha256 "13858ef1018f41b93026302840e832c2b65289242225c5a19ce5e26f84607f15"
  license all_of: ["BSD-4-Clause-UC", "APSL-1.0"]

  livecheck do
    url "https://opensource.apple.com/tarballs/remote_cmds/"
    regex(/href=.*?remote_cmds[._-]v?(\d+(?:\.\d+)*)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "e6fb7de53e703755a72e227752f81023c2935567d935af638959e986da910b3e" => :big_sur
    sha256 "563fcbef08590e48a07079f00877e14f22f54849db11ee02ba7c935499a7ccd1" => :arm64_big_sur
    sha256 "7435a9fd2515158762a85197a4ad7141e430383e185e002da169dbbb638c952f" => :catalina
    sha256 "d5009f496dc6cf0c13b936996f98b91b0f12733ea9462843b56a39fc53b20fe0" => :mojave
    sha256 "af38f3c6dd4ff5eda2248671958e66595b39e74cdeecca52af4efb495bc659a7" => :high_sierra
  end

  depends_on xcode: :build

  conflicts_with "inetutils", because: "both install 'telnet' binaries"

  resource "libtelnet" do
    url "https://opensource.apple.com/tarballs/libtelnet/libtelnet-13.tar.gz"
    sha256 "e7d203083c2d9fa363da4cc4b7377d4a18f8a6f569b9bcf58f97255941a2ebd1"
  end

  def install
    resource("libtelnet").stage do
      ENV["SDKROOT"] = MacOS.sdk_path
      ENV["MACOSX_DEPLOYMENT_TARGET"] = MacOS.version

      xcodebuild "SYMROOT=build", "-arch", Hardware::CPU.arch

      libtelnet_dst = buildpath/"telnet.tproj/build/Products"
      libtelnet_dst.install "build/Release/libtelnet.a"
      libtelnet_dst.install "build/Release/usr/local/include/libtelnet/"
    end

    system "make", "-C", "telnet.tproj",
                   "OBJROOT=build/Intermediates",
                   "SYMROOT=build/Products",
                   "DSTROOT=build/Archive",
                   "CFLAGS=$(CC_Flags) -isystembuild/Products/",
                   "LDFLAGS=$(LD_Flags) -Lbuild/Products/",
                   "RC_ARCHS=#{Hardware::CPU.arch}",
                   "install"

    bin.install "telnet.tproj/build/Archive/usr/local/bin/telnet"
    man1.install "telnet.tproj/telnet.1"
  end

  test do
    output = shell_output("#{bin}/telnet india.colorado.edu 13", 1)
    assert_match "Connected to india.colorado.edu.", output
  end
end
