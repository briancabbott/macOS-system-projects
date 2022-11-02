class Ipbt < Formula
  desc "Program for recording a UNIX terminal session"
  homepage "https://www.chiark.greenend.org.uk/~sgtatham/ipbt/"
  url "https://www.chiark.greenend.org.uk/~sgtatham/ipbt/ipbt-20190601.d1519e0.tar.gz"
  version "20190601"
  sha256 "a519507fccda5e3054d3639e9abedb482a108fa8ee6fc3b1c03ba0d6a4ba48aa"

  livecheck do
    url :homepage
    regex(/href=.*?ipbt[._-]v?(\d+)(?:\.[\da-z]+)?\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "136112dbee67f57c38adf83a55913fff16c4444d7c849b80be5c463f5e2efc76" => :big_sur
    sha256 "e9b51c4509c77561acb5e1d2d90dd0b0e0e64b13c15bc3238e508995eb6c782a" => :arm64_big_sur
    sha256 "367536bc0020cd8b4313936070ec9539bcfe56de061a40b6bfc4aa0533d82a5a" => :catalina
    sha256 "e68f7a1286319ca19382bef65cbf2d80fd1f15bc46dc623cbe9b8f73b5d9d848" => :mojave
    sha256 "9ae5d95807ead91cb2bd746fb2f3d4fee82cb39dba42f67bdea9eede792b7261" => :high_sierra
  end

  uses_from_macos "ncurses"

  def install
    system "./configure", "--prefix=#{prefix}",
                          "--disable-dependency-tracking"
    system "make", "install"
  end

  test do
    system "#{bin}/ipbt"
  end
end
