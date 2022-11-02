class Slacknimate < Formula
  desc "Text animation for Slack messages"
  homepage "https://github.com/mroth/slacknimate"
  url "https://github.com/mroth/slacknimate/archive/v1.1.0.tar.gz"
  sha256 "71c7a65192c8bbb790201787fabbb757de87f8412e0d41fe386c6b4343cb845c"
  license "MPL-2.0"
  head "https://github.com/mroth/slacknimate.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "d8120fd0cedd32b5be89ff29f2eed08d060a810820cfc23f6f74e1c7201ff5ad" => :big_sur
    sha256 "35f24a47ca03293bec53b2b622cc1c6f0a012b5c674c0fea83a79795474caefb" => :arm64_big_sur
    sha256 "52bd6b01115cb8e84d3479ff6dea669a98b17b60cc6090b3384ac44fdcbdd93a" => :catalina
    sha256 "28f1871e38987c5b06e0666f172d0eefb9e6895ea8207a0ad171d467a2df7f7a" => :mojave
    sha256 "6849d5acbe802d8fb69007f144bba62a9c259a9093ccc920fb9a200edc9368fa" => :high_sierra
  end

  depends_on "go" => :build

  def install
    system "go", "build",
      "-ldflags", "-s -w -X main.version=#{version}", *std_go_args, "./cmd/slacknimate"
  end

  test do
    system "#{bin}/slacknimate", "--version"
    system "#{bin}/slacknimate", "--help"
  end
end
