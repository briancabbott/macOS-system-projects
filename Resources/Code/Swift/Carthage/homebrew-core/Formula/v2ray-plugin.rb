class V2rayPlugin < Formula
  desc "SIP003 plugin based on v2ray for shadowsocks"
  homepage "https://github.com/shadowsocks/v2ray-plugin"
  url "https://github.com/shadowsocks/v2ray-plugin/archive/v1.3.1.tar.gz"
  sha256 "86d37a8ecef82457b4750a1af9e8d093b25ae0d32ea7dcc2ad5c0068fe2d3d74"
  license "MIT"
  head "https://github.com/shadowsocks/v2ray-plugin.git"

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "6a3064ead8cb35a8951619e5899f4ddbeff48a0e504bf156d8325079fe5c642a" => :big_sur
    sha256 "8ff4ac95fa05cc7d11429495c27eb499a6b2539fc6306eda02593a3dbd2c3b9a" => :arm64_big_sur
    sha256 "891f541e150a393ff20caa78eb79ef12f60929fb9e5b35826e2e639c46a61dc2" => :catalina
    sha256 "cb8ff7b812aa561f9e23935461968ba1c26cbe393c599aab4e1753b37702748b" => :mojave
    sha256 "f11b330c3dc9c445b757188057c93ce94de89f03f4adfa1a8c6405f5ba66b400" => :high_sierra
  end

  depends_on "go" => :build

  def install
    system "go", "build", "-ldflags", "-X main.VERSION=v#{version}", "-o", bin/"v2ray-plugin"
  end

  test do
    server = fork do
      exec bin/"v2ray-plugin", "-localPort", "54000", "-remoteAddr", "github.com", "-remotePort", "80", "-server"
    end
    client = fork do
      exec bin/"v2ray-plugin", "-localPort", "54001", "-remotePort", "54000"
    end
    sleep 2
    begin
      system "curl", "localhost:54001"
    ensure
      Process.kill 9, server
      Process.wait server
      Process.kill 9, client
      Process.wait client
    end
  end
end
