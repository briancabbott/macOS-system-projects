class Rtags < Formula
  desc "Source code cross-referencer like ctags with a clang frontend"
  homepage "https://github.com/Andersbakken/rtags"
  url "https://github.com/Andersbakken/rtags.git",
      tag:      "v2.38",
      revision: "9687ccdb9e539981e7934e768ea5c84464a61139"
  license "GPL-3.0"
  head "https://github.com/Andersbakken/rtags.git"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any
    sha256 "8412892ed1cfce17e4575a7bad34fd208fcc80d44b263460bb75c2d8d9346f3c" => :big_sur
    sha256 "7db38c61d8d0df69ed9dfa3934ca55480b2b84bea813b6be9fed3d74c11b00be" => :arm64_big_sur
    sha256 "332ba278034061d8789e8bcfc2d06120c122f0912de030524ee44d73089bdda6" => :catalina
    sha256 "a9b3b3f280643e151a9d98438ae1bef2bf77eda3a3412d07c1781d60b6e13a25" => :mojave
    sha256 "b1f34a462f2473d7059b8db4d78ff85f3bc18e5df25e2d597ce95052d15da132" => :high_sierra
  end

  depends_on "cmake" => :build
  depends_on "emacs"
  depends_on "llvm"
  depends_on "openssl@1.1"

  def install
    # Homebrew llvm libc++.dylib doesn't correctly reexport libc++abi
    ENV.append("LDFLAGS", "-lc++abi")

    args = std_cmake_args << "-DRTAGS_NO_BUILD_CLANG=ON"

    mkdir "build" do
      system "cmake", "..", *args
      system "make"
      system "make", "install"
    end
  end

  plist_options manual: "#{HOMEBREW_PREFIX}/bin/rdm --verbose --inactivity-timeout=300 --log-file=#{HOMEBREW_PREFIX}/var/log/rtags.log"

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
          <string>#{bin}/rdm</string>
          <string>--verbose</string>
          <string>--launchd</string>
          <string>--inactivity-timeout=300</string>
          <string>--log-file=#{var}/log/rtags.log</string>
        </array>
        <key>Sockets</key>
        <dict>
          <key>Listener</key>
          <dict>
            <key>SockPathName</key>
            <string>#{ENV["HOME"]}/.rdm</string>
          </dict>
        </dict>
      </dict>
      </plist>
    EOS
  end

  test do
    mkpath testpath/"src"
    (testpath/"src/foo.c").write <<~EOS
      void zaphod() {
      }

      void beeblebrox() {
        zaphod();
      }
    EOS
    (testpath/"src/README").write <<~EOS
      42
    EOS

    rdm = fork do
      $stdout.reopen("/dev/null")
      $stderr.reopen("/dev/null")
      exec "#{bin}/rdm", "--exclude-filter=\"\"", "-L", "log"
    end

    begin
      sleep 1
      pipe_output("#{bin}/rc -c", "clang -c #{testpath}/src/foo.c", 0)
      sleep 1
      assert_match "foo.c:1:6", shell_output("#{bin}/rc -f #{testpath}/src/foo.c:5:3")
      system "#{bin}/rc", "-q"
    ensure
      Process.kill 9, rdm
      Process.wait rdm
    end
  end
end
