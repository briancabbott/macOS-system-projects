class V2ray < Formula
  desc "Platform for building proxies to bypass network restrictions"
  homepage "https://v2fly.org/"
  url "https://github.com/v2fly/v2ray-core/archive/v4.34.0.tar.gz"
  sha256 "b250f569cb0369f394f63184e748f1df0c90500feb8a1bf2276257c4c8b81bee"
  license all_of: ["MIT", "CC-BY-SA-4.0"]
  head "https://github.com/v2fly/v2ray-core.git"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "30b67c553f0f59f6f44318738c3f7e7dc7f0f88757950d0bebcdcba0356ee5c7" => :big_sur
    sha256 "5fbeedfddb8106c482fe9f1c5e03e1f6688cb19f95d5937b41173e9c609cec55" => :catalina
    sha256 "78daf9a12daffce6a9002d5b58bf17553c072a4be58ffd0fb84653c45c2614bb" => :mojave
  end

  depends_on "go" => :build

  resource "geoip" do
    url "https://github.com/v2fly/geoip/releases/download/202011190012/geoip.dat"
    sha256 "022e6426f66cd7093fc2454c28537d2345b4fce49dc97b81ddfec07ce54e7081"
  end

  resource "geosite" do
    url "https://github.com/v2fly/domain-list-community/releases/download/20201122065644/dlc.dat"
    sha256 "574af5247bb83db844be03038c8fed1e488bf4bd4ce5de2843847cf40be923c1"
  end

  def install
    ldflags = "-s -w -buildid="
    system "go", "build", *std_go_args,
                 "-ldflags", ldflags,
                 "./main"
    system "go", "build", *std_go_args,
                 "-ldflags", ldflags,
                 "-tags", "confonly",
                 "-o", bin/"v2ctl",
                 "./infra/control/main"

    pkgetc.install "release/config/config.json" => "config.json"

    resource("geoip").stage do
      pkgshare.install "geoip.dat"
    end

    resource("geosite").stage do
      pkgshare.install "dlc.dat" => "geosite.dat"
    end
  end

  plist_options manual: "v2ray -config=#{HOMEBREW_PREFIX}/etc/v2ray/config.json"

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
            <string>#{bin}/v2ray</string>
            <string>-config</string>
            <string>#{etc}/v2ray/config.json</string>
          </array>
          <key>KeepAlive</key>
          <true/>
          <key>RunAtLoad</key>
          <true/>
        </dict>
      </plist>
    EOS
  end

  test do
    (testpath/"config.json").write <<~EOS
      {
        "log": {
          "access": "#{testpath}/log"
        },
        "outbounds": [
          {
            "protocol": "freedom",
            "tag": "direct"
          }
        ],
        "routing": {
          "rules": [
            {
              "ip": [
                "geoip:private"
              ],
              "outboundTag": "direct",
              "type": "field"
            }
          ]
        }
      }
    EOS
    output = shell_output "#{bin}/v2ray -c #{testpath}/config.json -test"

    assert_match "Configuration OK", output
    assert_predicate testpath/"log", :exist?
  end
end
