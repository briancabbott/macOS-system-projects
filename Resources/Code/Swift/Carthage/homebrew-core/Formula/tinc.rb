class Tinc < Formula
  desc "Virtual Private Network (VPN) tool"
  homepage "https://www.tinc-vpn.org/"
  url "https://tinc-vpn.org/packages/tinc-1.0.36.tar.gz"
  sha256 "40f73bb3facc480effe0e771442a706ff0488edea7a5f2505d4ccb2aa8163108"
  license "GPL-2.0-or-later"

  livecheck do
    url "https://www.tinc-vpn.org/download/"
    regex(/href=.*?tinc[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "5059a3bb30eaf4d339742ade7272a838008786010e709781bdf28856131229d5" => :big_sur
    sha256 "697c1a6266c837ae6f32ef0509b6d5770c7f723e5791b3263693fc8429baeb25" => :arm64_big_sur
    sha256 "fcaaca6b5abf4f30a55149f41871a7c4ec99fe8a9dc87ddb68ea735c03569a66" => :catalina
    sha256 "ba34dc41517f617c4d61d61e2f76cbafd9b75aa5edacc894e5b24e97cfb269f5" => :mojave
    sha256 "a5ec2ae5f1e6252f80f33158bb0a1140e29764ed1f2e8754dcedf50e4fb49290" => :high_sierra
    sha256 "923b15d1dcd7aafbb566f83edc9cced61b13379e857243bbe28b2755270b542d" => :sierra
  end

  depends_on "lzo"
  depends_on "openssl@1.1"

  uses_from_macos "zlib"

  def install
    system "./configure", "--prefix=#{prefix}", "--sysconfdir=#{etc}",
                          "--with-openssl=#{Formula["openssl@1.1"].opt_prefix}"
    system "make", "install"
  end

  test do
    assert_match version.to_s, shell_output("#{sbin}/tincd --version")
  end
end
