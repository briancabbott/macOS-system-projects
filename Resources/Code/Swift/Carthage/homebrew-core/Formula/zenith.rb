class Zenith < Formula
  desc "In terminal graphical metrics for your *nix system"
  homepage "https://github.com/bvaisvil/zenith/"
  url "https://github.com/bvaisvil/zenith/archive/0.12.0.tar.gz"
  sha256 "2b33892be95149550c84179b341e304c4222e3489bc121ea8c8346e075433aa6"
  license "MIT"
  version_scheme 1
  head "https://github.com/bvaisvil/zenith.git"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "eeb7ca6be51f678aaad01f4025ad62faf0068f48a6b8ef1eb908cb37676861e8" => :big_sur
    sha256 "7d883fde3bda035e36d3107923e96362cb5e538753ad4366ac8fa04137143cb2" => :arm64_big_sur
    sha256 "929ff75d609fec1abe20a6fe0e1833949b0d1157821be5778c91b809a20d3193" => :catalina
    sha256 "8e587b9ae59e970e0b3ee7a7e8f6076cdde1c002e81d61a2e373ca7bd8942443" => :mojave
  end

  depends_on "rust" => :build

  def install
    system "cargo", "install", *std_cargo_args
  end

  test do
    require "pty"
    require "io/console"

    (testpath/"zenith").mkdir
    r, w, pid = PTY.spawn "#{bin}/zenith --db zenith"
    r.winsize = [80, 43]
    sleep 1
    w.write "q"
    assert_match /PID\s+USER\s+P\s+N\s+↓CPU%\s+MEM%/, r.read
  ensure
    Process.kill("TERM", pid)
  end
end
