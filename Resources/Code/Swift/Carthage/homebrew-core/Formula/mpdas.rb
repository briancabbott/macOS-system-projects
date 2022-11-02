class Mpdas < Formula
  desc "C++ client to submit tracks to audioscrobbler"
  homepage "https://www.50hz.ws/mpdas/"
  url "https://www.50hz.ws/mpdas/mpdas-0.4.5.tar.gz"
  sha256 "c9103d7b897e76cd11a669e1c062d74cb73574efc7ba87de3b04304464e8a9ca"
  license "BSD-3-Clause"
  head "https://github.com/hrkfdn/mpdas.git"

  bottle do
    sha256 "91ac2d2c0a96b2a91becbf3f6d3f34f01031ed507efce5e5a6eca0d3cdabc41c" => :big_sur
    sha256 "88b82424c0013b5c0261fe9f08aea489466a33b59321878f19b924325e25bdf9" => :arm64_big_sur
    sha256 "db93645db3fef2737193f310b8261a435ad79c426e186c6127017b37cc81ef66" => :catalina
    sha256 "448514d6ac177e771f61bcd178550e317560cf3d5d73bfd240c3278d8d3f5193" => :mojave
    sha256 "ae319b22981a8cc5ed9a0e0212f2ecdbd7660bcd32182334865a01ac69c2832f" => :high_sierra
    sha256 "06fe51aaa95bfd3000f1f9e562709d266ecbf1880d2b96779ff0c9b9d82dea20" => :sierra
    sha256 "c9261f50d1d71969474203f6431d7902198c3524d828ed6f690733094444a914" => :el_capitan
  end

  depends_on "pkg-config" => :build
  depends_on "libmpdclient"

  def install
    system "make", "PREFIX=#{prefix}", "MANPREFIX=#{man1}", "CONFIG=#{etc}", "install"
    etc.install "mpdasrc.example"
  end

  plist_options manual: "mpdas"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
      <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>WorkingDirectory</key>
          <string>#{HOMEBREW_PREFIX}</string>
          <key>ProgramArguments</key>
          <array>
              <string>#{opt_bin}/mpdas</string>
          </array>
          <key>RunAtLoad</key>
          <true/>
          <key>KeepAlive</key>
          <true/>
      </dict>
      </plist>
    EOS
  end

  test do
    system bin/"mpdas", "-v"
  end
end
