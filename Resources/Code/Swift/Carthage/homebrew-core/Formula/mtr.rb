class Mtr < Formula
  desc "'traceroute' and 'ping' in a single tool"
  homepage "https://www.bitwizard.nl/mtr/"
  url "https://github.com/traviscross/mtr/archive/v0.94.tar.gz"
  sha256 "ea036fdd45da488c241603f6ea59a06bbcfe6c26177ebd34fff54336a44494b8"
  license "GPL-2.0-only"
  head "https://github.com/traviscross/mtr.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "3625ac3eeb2409adfb156194c8bda385b98ebb7afa59229e302e9e61bb897004" => :big_sur
    sha256 "2e6e4509d128d1f3bc7e0594c8fb12b60971780e070523a2e4d8b896507cef57" => :arm64_big_sur
    sha256 "6ec962809c1c2381b7723647e16c2283dbef8042ae04af14ad675fa63c38a859" => :catalina
    sha256 "6a768b9cd07026aec0276742fd3fb6723c0f545a8498dff1ab3bb5b9e23e85e0" => :mojave
    sha256 "9c9a9c995360d16581ef42b0a729a5d3c152e7195bcc88910cda9bd9315c3299" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "pkg-config" => :build

  def install
    # Fix UNKNOWN version reported by `mtr --version`.
    inreplace "configure.ac",
              "m4_esyscmd([build-aux/git-version-gen .tarball-version])",
              version.to_s

    # We need to add this because nameserver8_compat.h has been removed in Snow Leopard
    ENV["LIBS"] = "-lresolv"
    args = %W[
      --disable-dependency-tracking
      --prefix=#{prefix}
      --without-glib
      --without-gtk
    ]
    system "./bootstrap.sh"
    system "./configure", *args
    system "make", "install"
  end

  def caveats
    <<~EOS
      mtr requires root privileges so you will need to run `sudo mtr`.
      You should be certain that you trust any software you grant root privileges.
    EOS
  end

  test do
    # mtr will not run without root privileges
    assert_match "Failure to open", shell_output("#{sbin}/mtr google.com 2>&1", 1)
  end
end
