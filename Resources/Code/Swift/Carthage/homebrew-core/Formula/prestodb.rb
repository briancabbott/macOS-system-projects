class Prestodb < Formula
  desc "Distributed SQL query engine for big data"
  homepage "https://prestodb.io"
  url "https://search.maven.org/remotecontent?filepath=com/facebook/presto/presto-server/0.245.1/presto-server-0.245.1.tar.gz"
  sha256 "8fbd4f46527bdc58198837cb791d3117074db058fdc64e4c798c2ea5856d02ae"
  license "Apache-2.0"

  # The source of the Presto download page at https://prestodb.io/download.html
  # contains old version information. The current version information is loaded
  # from the JavaScript file below, so we check that instead. We don't check
  # Maven because sometimes the directory listing page contains a newer version
  # that hasn't been released yet and we probably don't want to upgrade until
  # it's official on the first-party website, etc.
  livecheck do
    url "https://prestodb.io/static/js/version.js"
    regex(/latest_presto_version.*?(\d+(?:\.\d+)+)/i)
  end

  bottle :unneeded

  depends_on "openjdk"

  conflicts_with "prestosql", because: "both install `presto` and `presto-server` binaries"

  resource "presto-cli" do
    url "https://search.maven.org/remotecontent?filepath=com/facebook/presto/presto-cli/0.245.1/presto-cli-0.245.1-executable.jar"
    sha256 "c6a8eab54d14f8e2f3798889f663dab701f6856e1455ee46717a3141ac528086"
  end

  def install
    libexec.install Dir["*"]

    (libexec/"etc/node.properties").write <<~EOS
      node.environment=production
      node.id=ffffffff-ffff-ffff-ffff-ffffffffffff
      node.data-dir=#{var}/presto/data
    EOS

    (libexec/"etc/jvm.config").write <<~EOS
      -server
      -Xmx16G
      -XX:+UseG1GC
      -XX:G1HeapRegionSize=32M
      -XX:+UseGCOverheadLimit
      -XX:+ExplicitGCInvokesConcurrent
      -XX:+HeapDumpOnOutOfMemoryError
      -XX:+ExitOnOutOfMemoryError
      -Djdk.attach.allowAttachSelf=true
    EOS

    (libexec/"etc/config.properties").write <<~EOS
      coordinator=true
      node-scheduler.include-coordinator=true
      http-server.http.port=8080
      query.max-memory=5GB
      query.max-memory-per-node=1GB
      discovery-server.enabled=true
      discovery.uri=http://localhost:8080
    EOS

    (libexec/"etc/log.properties").write "com.facebook.presto=INFO"

    (libexec/"etc/catalog/jmx.properties").write "connector.name=jmx"

    (bin/"presto-server").write_env_script libexec/"bin/launcher", Language::Java.overridable_java_home_env

    resource("presto-cli").stage do
      libexec.install "presto-cli-#{version}-executable.jar"
      bin.write_jar_script libexec/"presto-cli-#{version}-executable.jar", "presto"
    end
  end

  def post_install
    (var/"presto/data").mkpath
  end

  def caveats
    <<~EOS
      Add connectors to #{opt_libexec}/etc/catalog/. See:
      https://prestodb.io/docs/current/connector.html
    EOS
  end

  plist_options manual: "presto-server run"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
      "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>RunAtLoad</key>
          <true/>
          <key>AbandonProcessGroup</key>
          <true/>
          <key>WorkingDirectory</key>
          <string>#{opt_libexec}</string>
          <key>ProgramArguments</key>
          <array>
            <string>#{opt_bin}/presto-server</string>
            <string>run</string>
          </array>
        </dict>
      </plist>
    EOS
  end

  test do
    system bin/"presto-server", "run", "--help"
    assert_match "Presto CLI #{version}", shell_output("#{bin}/presto --version").chomp
  end
end
