class Pwnat < Formula
  desc "Proxy server that works behind a NAT"
  homepage "https://samy.pl/pwnat/"
  url "https://samy.pl/pwnat/pwnat-0.3-beta.tgz"
  sha256 "d5d6ea14f1cf0d52e4f946be5c3630d6440f8389e7467c0117d1fe33b9d130a2"
  license "GPL-3.0"
  head "https://github.com/samyk/pwnat.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "84fa7c77483a6aaeaa15e2b6514478bc0c1a292c001cf1e7a1fa82490a7dfd0c" => :big_sur
    sha256 "25630303a2d2434a840a95274ff3063d8cdc702d00d31907d12cc8f1befb7180" => :arm64_big_sur
    sha256 "51a038c40431d552b19beb59e14de49984923d216b5d411646a32a2caed1eff8" => :catalina
    sha256 "483e476edd037e89dc1c24fbf115132a815a9a4d6fcb987ceea6a4c07a5944da" => :mojave
    sha256 "3a4bf09acd5eda4e54fbe21d0028948613fa398a3a1272722957079a9f18c836" => :high_sierra
    sha256 "f8319cece67a334c14129e706f9d1b249d7905cf1ad62df9b5ee9553dbb8d001" => :sierra
    sha256 "0149fc977622f2fd55db5845a377437028df31bb847230d3fd73d548e481e289" => :el_capitan
    sha256 "cf17568c4053240ffe61594bcc618577c0d0c569abda8b3b956a4e4b441a755e" => :yosemite
    sha256 "0baed31dc05b28a330501a0d4119e8997c1038d14311c64f2d7b367ebdf9f01e" => :mavericks
  end

  def install
    system "make", "CC=#{ENV.cc}", "CFLAGS=#{ENV.cflags}", "LDFLAGS=-lz"
    bin.install "pwnat"
  end

  test do
    shell_output("#{bin}/pwnat -h", 1)
  end
end
