class Watchman < Formula
  desc "Watch files and take action when they change"
  homepage "https://github.com/facebook/watchman"
  license "Apache-2.0"
  revision 5
  head "https://github.com/facebook/watchman.git"

  stable do
    url "https://github.com/facebook/watchman/archive/v4.9.0.tar.gz"
    sha256 "1f6402dc70b1d056fffc3748f2fdcecff730d8843bb6936de395b3443ce05322"

    # Upstream commit from 1 Sep 2017: "Have bin scripts use iter() method for python3"
    patch do
      url "https://github.com/facebook/watchman/commit/17958f7d.patch?full_index=1"
      sha256 "73990f0c7bd434d04fd5f1310b97c5f8599793007bd31ae438c2ba0211fb2c43"
    end
  end

  # The Git repo contains a few tags like `2020.05.18.00`, so we have to
  # restrict matching to versions with two to three parts (e.g., 1.2, 1.2.3).
  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+){,2})$/i)
  end

  bottle do
    sha256 "f03c91e17cd7595f98106ee4a27f28433ecc2fd6dde8cc1b7e279bd60b730051" => :big_sur
    sha256 "4a44a39cfd719b34d146043aa5afcc6ac304ebbd2ff4ff0fb2e37e22871f38ac" => :arm64_big_sur
    sha256 "30ed7115aa2a2534f5255508915f827c2e6f3100fcd7842415db64e31eabac30" => :catalina
    sha256 "135eb0a8f098417a8e4d67bf8d732a19bad1932eee085497877e93982e91074f" => :mojave
    sha256 "e872c3aae64c3b78197de9f12e272bebd5d20c316a120916f59a5f1cd2fac039" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"
  depends_on "pcre"
  depends_on "python@3.9"

  def install
    system "./autogen.sh"
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--with-pcre",
                          # Do homebrew specific Python installation below
                          "--without-python",
                          "--enable-statedir=#{var}/run/watchman"
    system "make"
    system "make", "install"

    # Homebrew specific python application installation
    python3 = Formula["python@3.9"].opt_bin/"python3"
    xy = Language::Python.major_minor_version python3
    ENV.prepend_create_path "PYTHONPATH", libexec/"lib/python#{xy}/site-packages"
    cd "python" do
      system python3, *Language::Python.setup_install_args(libexec)
    end
    bin.install Dir[libexec/"bin/*"]
    bin.env_script_all_files(libexec/"bin", PYTHONPATH: ENV["PYTHONPATH"])
  end

  def post_install
    (var/"run/watchman").mkpath
    chmod 042777, var/"run/watchman"
    # Older versions would use socket activation in the launchd.plist, and since
    # the homebrew paths are versioned, this meant that launchd would continue
    # to reference the old version of the binary after upgrading.
    # https://github.com/facebook/watchman/issues/358
    # To help swing folks from an older version to newer versions, force unloading
    # the plist here.  This is needed even if someone wanted to add brew services
    # support; while there are still folks with watchman <4.8 this is still an
    # upgrade concern.
    home = ENV["HOME"]
    system "launchctl", "unload",
           "-F", "#{home}/Library/LaunchAgents/com.github.facebook.watchman.plist"
  end

  test do
    assert_equal /(\d+\.\d+\.\d+)/.match(version)[0], shell_output("#{bin}/watchman -v").chomp
  end
end
