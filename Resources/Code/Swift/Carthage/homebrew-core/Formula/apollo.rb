class Apollo < Formula
  desc "Multi-protocol messaging broker based on ActiveMQ"
  homepage "https://activemq.apache.org/"
  url "https://www.apache.org/dyn/closer.lua?path=activemq/activemq-apollo/1.7.1/apache-apollo-1.7.1-unix-distro.tar.gz"
  mirror "https://archive.apache.org/dist/activemq/activemq-apollo/1.7.1/apache-apollo-1.7.1-unix-distro.tar.gz"
  sha256 "74577339a1843995a5128d14c68b21fb8f229d80d8ce1341dd3134f250ab689d"
  license "Apache-2.0"
  revision 1

  bottle do
    cellar :any_skip_relocation
    rebuild 2
    sha256 "b4ecc23c2aa054e69b8de5531d80315b5ed2746ea7cd438e66317bc666903a8b" => :big_sur
    sha256 "81b2a6a1110da6cf58c6725eb6e2c331668fa39d01644e0a754a2eb9241fdccd" => :catalina
    sha256 "81b2a6a1110da6cf58c6725eb6e2c331668fa39d01644e0a754a2eb9241fdccd" => :mojave
    sha256 "81b2a6a1110da6cf58c6725eb6e2c331668fa39d01644e0a754a2eb9241fdccd" => :high_sierra
  end

  # https://github.com/apache/activemq-apollo/commit/049d68bf3f94cdf62ded5426d3cad4ef3e3c56ca
  deprecate! date: "2019-03-11", because: :deprecated_upstream

  depends_on "openjdk"

  # https://www.oracle.com/technetwork/database/berkeleydb/overview/index-093405.html
  resource "bdb-je" do
    url "https://download.oracle.com/maven/com/sleepycat/je/5.0.34/je-5.0.34.jar"
    sha256 "025afa4954ed4e6f926af6e9015aa109528b0f947fcb3790b7bace639fe558fa"
  end

  # https://github.com/fusesource/fuse-extra/tree/master/fusemq-apollo/fusemq-apollo-mqtt
  resource "mqtt" do
    url "https://search.maven.org/remotecontent?filepath=org/fusesource/fuse-extra/fusemq-apollo-mqtt/1.3/fusemq-apollo-mqtt-1.3-uber.jar"
    sha256 "2795caacbc6086c7de46b588d11a78edbf8272acb7d9da3fb329cb34fcb8783f"
  end

  def install
    prefix.install_metafiles
    prefix.install %w[docs examples]
    libexec.install Dir["*"]

    (libexec/"lib").install resource("bdb-je")
    (libexec/"lib").install resource("mqtt")

    (bin/"apollo").write_env_script libexec/"bin/apollo", JAVA_HOME: Formula["openjdk"].opt_prefix
  end

  def caveats
    <<~EOS
      To create the broker:
        #{bin}/apollo create #{var}/apollo
    EOS
  end

  plist_options manual: "#{HOMEBREW_PREFIX}/var/apollo/bin/apollo-broker run"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>KeepAlive</key>
          <true/>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>ProgramArguments</key>
          <array>
            <string>#{var}/apollo/bin/apollo-broker</string>
            <string>run</string>
          </array>
          <key>RunAtLoad</key>
          <true/>
          <key>WorkingDirectory</key>
          <string>#{var}/apollo</string>
        </dict>
      </plist>
    EOS
  end

  test do
    system bin/"apollo", "create", testpath
    assert_predicate testpath/"bin/apollo-broker", :exist?
    assert_predicate testpath/"bin/apollo-broker", :executable?
  end
end
