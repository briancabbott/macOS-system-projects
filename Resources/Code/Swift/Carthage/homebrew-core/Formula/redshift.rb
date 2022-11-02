class Redshift < Formula
  desc "Adjust color temperature of your screen according to your surroundings"
  homepage "http://jonls.dk/redshift/"
  url "https://github.com/jonls/redshift/releases/download/v1.12/redshift-1.12.tar.xz"
  sha256 "d2f8c5300e3ce2a84fe6584d2f1483aa9eadc668ab1951b2c2b8a03ece3a22ba"
  license "GPL-3.0"
  revision 1

  bottle do
    sha256 "0d12842206f6644ec971b204ecf5d4889f868e3c26f9596e541f1977eb901feb" => :big_sur
    sha256 "2c802664981ccedd90f69895e4389461fb631acc1766226b1d8ff3bc51be7988" => :arm64_big_sur
    sha256 "b40870e8bcb3d28fdc6fa5a1d7c232939973e4a73a38029afd0bc6f86c199b51" => :catalina
    sha256 "197ca4060616fbb79a6e64b93760f60ef581d5d76f838ab099b97076e3e569fe" => :mojave
    sha256 "f07311c326eb8c2310d509ffbcb5424d7783a1b0b675d47ac32026116086a39d" => :high_sierra
    sha256 "89ab02396a2d3694923f8496217a5d5a47c1cc35e167205cf4bb74033de92ab3" => :sierra
  end

  head do
    url "https://github.com/jonls/redshift.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "glib"

  def install
    args = %W[
      --prefix=#{prefix}
      --enable-corelocation
      --disable-silent-rules
      --disable-dependency-tracking
      --disable-geoclue
      --disable-geoclue2
      --enable-quartz
      --with-systemduserunitdir=no
      --disable-gui
    ]

    system "./bootstrap" if build.head?
    system "./configure", *args
    system "make", "install"
    pkgshare.install "redshift.conf.sample"
  end

  def caveats
    <<~EOS
      A sample .conf file has been installed to #{opt_pkgshare}.

      Please note redshift expects to read its configuration file from
      #{ENV["HOME"]}/.config/redshift/redshift.conf
    EOS
  end

  plist_options manual: "redshift"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>ProgramArguments</key>
          <array>
            <string>#{opt_bin}/redshift</string>
          </array>
          <key>KeepAlive</key>
          <true/>
          <key>RunAtLoad</key>
          <true/>
          <key>StandardErrorPath</key>
          <string>/dev/null</string>
          <key>StandardOutPath</key>
          <string>/dev/null</string>
        </dict>
      </plist>
    EOS
  end

  test do
    system "#{bin}/redshift", "-V"
  end
end
