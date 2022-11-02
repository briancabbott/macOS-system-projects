class Openconnect < Formula
  desc "Open client for Cisco AnyConnect VPN"
  homepage "https://www.infradead.org/openconnect/"
  url "ftp://ftp.infradead.org/pub/openconnect/openconnect-8.10.tar.gz"
  mirror "https://fossies.org/linux/privat/openconnect-8.10.tar.gz"
  sha256 "30e64c6eca4be47bbf1d61f53dc003c6621213738d4ea7a35e5cf1ac2de9bab1"
  license "LGPL-2.1-only"

  livecheck do
    url "https://www.infradead.org/openconnect/download.html"
    regex(/href=.*?openconnect[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "9755c4ea66ed9c8aa1f1ee966c932ec2be37849887636d8f65a920f20c16ec55" => :big_sur
    sha256 "94132025cfdc325c792b5eed39e3afe8f86bf4512e06379d8374aabd72364115" => :arm64_big_sur
    sha256 "b4144970e695adc8f049319408cd431c96eb2ca4714feb903e0f01f3926dfd1f" => :catalina
    sha256 "5f4d9cb8a0a39983205bad4e1e6d7a2ae586f0725571fa83eac6421b8d6f4b9a" => :mojave
    sha256 "4d306766b4a334c7dcc8497b0684005c9011cd8913131b25bae2f56f3b3217d1" => :high_sierra
  end

  head do
    url "git://git.infradead.org/users/dwmw2/openconnect.git", shallow: false
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "gnutls"
  depends_on "stoken"

  resource "vpnc-script" do
    url "https://git.infradead.org/users/dwmw2/vpnc-scripts.git/blob_plain/c0122e891f7e033f35f047dad963702199d5cb9e:/vpnc-script"
    sha256 "3ddd9d6b46e92d76e6e26d89447e3a82d797ecda125d31792f14c203742dea0f"
  end

  def install
    etc.install resource("vpnc-script")
    chmod 0755, "#{etc}/vpnc-script"

    if build.head?
      ENV["LIBTOOLIZE"] = "glibtoolize"
      system "./autogen.sh"
    end

    args = %W[
      --prefix=#{prefix}
      --sbindir=#{bin}
      --localstatedir=#{var}
      --with-vpnc-script=#{etc}/vpnc-script
    ]

    system "./configure", *args
    system "make", "install"
  end

  test do
    assert_match "POST https://localhost/", pipe_output("#{bin}/openconnect localhost 2>&1")
  end
end
