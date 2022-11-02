class Pidgin < Formula
  desc "Multi-protocol chat client"
  homepage "https://pidgin.im/"
  url "https://downloads.sourceforge.net/project/pidgin/Pidgin/2.14.1/pidgin-2.14.1.tar.bz2"
  sha256 "f132e18d551117d9e46acce29ba4f40892a86746c366999166a3862b51060780"
  license "GPL-2.0-or-later"
  revision 1

  livecheck do
    url :stable
    regex(%r{url=.*?/pidgin[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    sha256 "ffbc6daf100dda1b80d898692dd77dbde42fe5223e74316252bed2173197f2ed" => :big_sur
    sha256 "d8ecf79354ce2ec2c060b58305743452f40e148a72ba5d72612b0f018ab00144" => :arm64_big_sur
    sha256 "c5dc52dceddf5a7084b84b3c9515a0d08149f934420a08b12bc7f89ed4b7afe0" => :catalina
    sha256 "14c679b8cca37387b9ca473d09b24fa89b61986bef017f94733a58a4fd97d069" => :mojave
  end

  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "cairo"
  depends_on "gettext"
  depends_on "gnutls"
  depends_on "gtk+"
  depends_on "libgcrypt"
  depends_on "libgnt"
  depends_on "libidn"
  depends_on "libotr"
  depends_on "pango"

  # Finch has an equal port called purple-otr but it is a NIGHTMARE to compile
  # If you want to fix this and create a PR on Homebrew please do so.
  resource "pidgin-otr" do
    url "https://otr.cypherpunks.ca/pidgin-otr-4.0.2.tar.gz"
    sha256 "f4b59eef4a94b1d29dbe0c106dd00cdc630e47f18619fc754e5afbf5724ebac4"
  end

  def install
    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --prefix=#{prefix}
      --disable-avahi
      --disable-dbus
      --disable-doxygen
      --disable-gevolution
      --disable-gstreamer
      --disable-gstreamer-interfaces
      --disable-gtkspell
      --disable-meanwhile
      --disable-vv
      --enable-gnutls=yes
      --with-tclconfig=#{MacOS.sdk_path}/System/Library/Frameworks/Tcl.framework
      --with-tkconfig=#{MacOS.sdk_path}/System/Library/Frameworks/Tk.framework
      --without-x
    ]

    ENV["ac_cv_func_perl_run"] = "yes" if MacOS.version == :high_sierra

    # patch pidgin to read plugins and allow them to live in separate formulae which can
    # all install their symlinks into these directories. See:
    #   https://github.com/Homebrew/homebrew-core/pull/53557
    inreplace "finch/finch.c", "LIBDIR", "\"#{HOMEBREW_PREFIX}/lib/finch\""
    inreplace "libpurple/plugin.c", "LIBDIR", "\"#{HOMEBREW_PREFIX}/lib/purple-2\""
    inreplace "pidgin/gtkmain.c", "LIBDIR", "\"#{HOMEBREW_PREFIX}/lib/pidgin\""
    inreplace "pidgin/gtkutils.c", "DATADIR", "\"#{HOMEBREW_PREFIX}/share\""

    system "./configure", *args
    system "make", "install"

    resource("pidgin-otr").stage do
      ENV.prepend "CFLAGS", "-I#{Formula["libotr"].opt_include}"
      ENV.append_path "PKG_CONFIG_PATH", "#{lib}/pkgconfig"
      system "./configure", "--prefix=#{prefix}", "--mandir=#{man}"
      system "make", "install"
    end
  end

  test do
    system "#{bin}/finch", "--version"
    system "#{bin}/pidgin", "--version"

    pid = fork { exec "#{bin}/pidgin", "--config=#{testpath}" }
    sleep 5
    Process.kill "SIGTERM", pid
  end
end
