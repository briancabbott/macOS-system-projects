class ApacheArchiva < Formula
  desc "Build Artifact Repository Manager"
  homepage "https://archiva.apache.org/"
  url "https://www.apache.org/dyn/closer.lua?path=archiva/2.2.5/binaries/apache-archiva-2.2.5-bin.tar.gz"
  mirror "https://archive.apache.org/dist/archiva/2.2.5/binaries/apache-archiva-2.2.5-bin.tar.gz"
  sha256 "01119af2d9950eacbcce0b7f8db5067b166ad26c1e1701bef829105441bb6e29"
  license "Apache-2.0"

  livecheck do
    url :stable
  end

  bottle :unneeded

  depends_on "openjdk"

  def install
    libexec.install Dir["*"]
    (bin/"archiva").write_env_script libexec/"bin/archiva", JAVA_HOME: Formula["openjdk"].opt_prefix
  end

  def post_install
    (var/"archiva/logs").mkpath
    (var/"archiva/data").mkpath
    (var/"archiva/temp").mkpath

    cp_r libexec/"conf", var/"archiva"
  end

  plist_options manual: "ARCHIVA_BASE=#{HOMEBREW_PREFIX}/var/archiva #{HOMEBREW_PREFIX}/opt/apache-archiva/bin/archiva console"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>ProgramArguments</key>
          <array>
            <string>#{opt_bin}/archiva</string>
            <string>console</string>
          </array>
          <key>Disabled</key>
          <false/>
          <key>RunAtLoad</key>
          <true/>
          <key>UserName</key>
          <string>archiva</string>
          <key>StandardOutPath</key>
          <string>#{var}/archiva/logs/launchd.log</string>
          <key>EnvironmentVariables</key>
          <dict>
            <key>ARCHIVA_BASE</key>
            <string>#{var}/archiva</string>
          </dict>
        </dict>
      </plist>
    EOS
  end

  test do
    assert_match "was not running.", shell_output("#{bin}/archiva stop")
  end
end
