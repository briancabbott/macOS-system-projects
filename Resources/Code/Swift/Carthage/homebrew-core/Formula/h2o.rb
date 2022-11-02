class H2o < Formula
  desc "HTTP server with support for HTTP/1.x and HTTP/2"
  homepage "https://github.com/h2o/h2o/"
  url "https://github.com/h2o/h2o/archive/v2.2.6.tar.gz"
  sha256 "f8cbc1b530d85ff098f6efc2c3fdbc5e29baffb30614caac59d5c710f7bda201"
  license "MIT"
  revision 1

  bottle do
    sha256 "00497d41695d9abaec982136824eb52b31aafc0727005f172ccf7510c41f0e65" => :big_sur
    sha256 "68375aeb216194e9731a44b1d794279e98c29f6280d4595e3ed0e5f2f40bdad0" => :arm64_big_sur
    sha256 "2a76dbab7292c0244c32e6a350f0c39dfb4d9b066de8510f2d8f3a9905c05f54" => :catalina
    sha256 "4f8f5c326d24dcfc95faf48849ae89721f1e19a407968cfa67efbc99dba33f76" => :mojave
    sha256 "80eac6a05ba27ce57142ad1a9211495fa3b044433623438b6319109e2852eb55" => :high_sierra
    sha256 "049e412820e6495cfb0906101cb00cea928543583cfc1b6986e0a52d1d215d0c" => :sierra
  end

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"

  uses_from_macos "zlib"

  def install
    # https://github.com/Homebrew/homebrew-core/pull/1046
    # https://github.com/Homebrew/brew/pull/251
    ENV.delete("SDKROOT")

    system "cmake", *std_cmake_args,
                    "-DWITH_BUNDLED_SSL=OFF",
                    "-DOPENSSL_ROOT_DIR=#{Formula["openssl@1.1"].opt_prefix}"
    system "make", "install"

    (etc/"h2o").mkpath
    (var/"h2o").install "examples/doc_root/index.html"
    # Write up a basic example conf for testing.
    (buildpath/"brew/h2o.conf").write conf_example
    (etc/"h2o").install buildpath/"brew/h2o.conf"
    pkgshare.install "examples"
  end

  # This is simplified from examples/h2o/h2o.conf upstream.
  def conf_example(port = 8080)
    <<~EOS
      listen: #{port}
      hosts:
        "127.0.0.1.xip.io:#{port}":
          paths:
            /:
              file.dir: #{var}/h2o/
    EOS
  end

  def caveats
    <<~EOS
      A basic example configuration file has been placed in #{etc}/h2o.

      You can find fuller, unmodified examples in #{opt_pkgshare}/examples.
    EOS
  end

  plist_options manual: "h2o"

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
          <true/>
          <key>ProgramArguments</key>
          <array>
              <string>#{opt_bin}/h2o</string>
              <string>-c</string>
              <string>#{etc}/h2o/h2o.conf</string>
          </array>
        </dict>
      </plist>
    EOS
  end

  test do
    port = free_port
    (testpath/"h2o.conf").write conf_example(port)
    fork do
      exec "#{bin}/h2o -c #{testpath}/h2o.conf"
    end
    sleep 2

    assert_match "Welcome to H2O", shell_output("curl localhost:#{port}")
  end
end
