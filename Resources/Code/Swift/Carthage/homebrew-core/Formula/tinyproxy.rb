class Tinyproxy < Formula
  desc "HTTP/HTTPS proxy for POSIX systems"
  homepage "https://tinyproxy.github.io/"
  url "https://github.com/tinyproxy/tinyproxy/releases/download/1.10.0/tinyproxy-1.10.0.tar.xz"
  sha256 "59be87689c415ba0d9c9bc6babbdd3df3b372d60b21e526b118d722dbc995682"
  license "GPL-2.0"
  revision 1

  bottle do
    sha256 "8c1d6bc6f48726ab8e40a87bed6afa11fb85b031ebf37dfb8b47f5ddc164e7ff" => :big_sur
    sha256 "6a56771b8b4fcfdea0bf0b23565eab216b1369dd5f77aa1bccd2a37cd46c392f" => :arm64_big_sur
    sha256 "e5a6e416b7f80da4a8e3af8ebaaf4e4c30d5f375845e44e72878170eeabffac0" => :catalina
    sha256 "fdf164a29e4730795b6b66fdabb34a35f34b91e4d8c896fa461542ec356d464d" => :mojave
    sha256 "05aed7a81fe9f92f043fe55ac10dba2474df664f710c01ee92283e5cf7fe0324" => :high_sierra
    sha256 "97cefacaaf1aa12eabe102ad86cee01c24f50f2a3ec07ca1eb17799319f02385" => :sierra
  end

  depends_on "asciidoc" => :build
  depends_on "docbook-xsl" => :build

  def install
    ENV["XML_CATALOG_FILES"] = "#{etc}/xml/catalog"

    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --disable-silent-rules
      --prefix=#{prefix}
      --localstatedir=#{var}
      --sysconfdir=#{etc}
      --disable-regexcheck
      --enable-filter
      --enable-reverse
      --enable-transparent
    ]

    system "./configure", *args
    system "make", "install"
  end

  def post_install
    (var/"log/tinyproxy").mkpath
    (var/"run/tinyproxy").mkpath
  end

  plist_options manual: "tinyproxy"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>RunAtLoad</key>
          <true/>
          <key>KeepAlive</key>
          <false/>
          <key>ProgramArguments</key>
          <array>
              <string>#{opt_bin}/tinyproxy</string>
              <string>-d</string>
          </array>
          <key>WorkingDirectory</key>
          <string>#{HOMEBREW_PREFIX}</string>
        </dict>
      </plist>
    EOS
  end

  test do
    port = free_port
    cp etc/"tinyproxy/tinyproxy.conf", testpath/"tinyproxy.conf"
    inreplace testpath/"tinyproxy.conf", "Port 8888", "Port #{port}"

    pid = fork do
      exec "#{bin}/tinyproxy", "-c", testpath/"tinyproxy.conf"
    end
    sleep 2

    begin
      assert_match /tinyproxy/, shell_output("curl localhost:#{port}")
    ensure
      Process.kill("SIGINT", pid)
      Process.wait(pid)
    end
  end
end
