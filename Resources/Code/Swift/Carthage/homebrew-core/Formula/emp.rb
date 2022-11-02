class Emp < Formula
  desc "CLI for Empire"
  homepage "https://github.com/remind101/empire"
  url "https://github.com/remind101/empire/archive/v0.13.0.tar.gz"
  sha256 "1294de5b02eaec211549199c5595ab0dbbcfdeb99f670b66e7890c8ba11db22b"
  license "BSD-2-Clause"

  bottle do
    cellar :any_skip_relocation
    sha256 "fc362d246942141f91da093183c54a8ff679bf263f0a4326d5bed7f94cbc8f59" => :big_sur
    sha256 "8c4bca6eca037bbef2b1a65d1974b43b36c81274e20597a76e87703ec477ee1a" => :catalina
    sha256 "33eafe903efc393c0964ac05ab684508b98e72a4ee2f26272ee16eee159cd514" => :mojave
    sha256 "d96c6b3f2ee49480ddc0dac10484284e7620dce5499482bdaf12c26f42f93a13" => :high_sierra
    sha256 "2a45cd98d7345ff1872137576f97a028729ff4c0d62994d1ce6d573e3835e9db" => :sierra
    sha256 "af64990b64d29f8383db471092279e9d039c7c81b6294099bb456890b6b5161b" => :el_capitan
  end

  depends_on "go" => :build

  def install
    ENV["GOPATH"] = buildpath

    (buildpath/"src/github.com/remind101/").mkpath
    ln_s buildpath, buildpath/"src/github.com/remind101/empire"

    system "go", "build", "-o", bin/"emp", "./src/github.com/remind101/empire/cmd/emp"
  end

  test do
    require "webrick"

    server = WEBrick::HTTPServer.new Port: 8035
    server.mount_proc "/apps/foo/releases" do |_req, res|
      resp = {
        "created_at"  => "2015-10-12T0:00:00.00000000-00:00",
        "description" => "my awesome release",
        "id"          => "v1",
        "user"        => {
          "id"    => "zab",
          "email" => "zab@waba.com",
        },
        "version"     => 1,
      }
      res.body = JSON.generate([resp])
    end

    Thread.new { server.start }

    begin
      ENV["EMPIRE_API_URL"] = "http://127.0.0.1:8035"
      assert_match /v1  zab  Oct 1(1|2|3)  2015  my awesome release/,
        shell_output("#{bin}/emp releases -a foo").strip
    ensure
      server.shutdown
    end
  end
end
