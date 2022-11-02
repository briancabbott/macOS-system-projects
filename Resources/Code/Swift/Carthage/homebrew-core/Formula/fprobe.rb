class Fprobe < Formula
  desc "Libpcap-based NetFlow probe"
  homepage "https://sourceforge.net/projects/fprobe/"
  url "https://downloads.sourceforge.net/project/fprobe/fprobe/1.1/fprobe-1.1.tar.bz2"
  sha256 "3a1cedf5e7b0d36c648aa90914fa71a158c6743ecf74a38f4850afbac57d22a0"
  license "GPL-2.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "b0a00f4b300f319155db2ce5c159dd5731147380cb4b21cdd001e7b519d132b2" => :big_sur
    sha256 "83c78c439cf2ec7338b3033e9cd623d04f8d19064ad566d206fc290d375f5472" => :arm64_big_sur
    sha256 "4684922307e7da6edc51c66f9ff647cf1d6b44bb75ab15deb4ea76629c8cbf2e" => :catalina
    sha256 "7867a9b5dc5014263723f471156464a641962249380494e4732785bafb7afeb2" => :mojave
    sha256 "31efd46250371cfd9ab386ff34cc41eb98d10758d550769ec72f8373ac1df800" => :high_sierra
    sha256 "fe38758d956c43b2a223734d426c990c63ee44ac643dc769c3d6a0cd4f07ef6b" => :sierra
    sha256 "9b06507a358024842b59c9f4d637b94b3681e720dbd3a1a8bc93d4d34f9a4442" => :el_capitan
    sha256 "18043cf3fcc930ee1690ee4bc74d92eed3c56a2424f85d58720c56a4b5bcad1d" => :yosemite
    sha256 "71b149edc078b237aecde23a53d76dc9809e3c9efafb32b52c03f3cc4af91c36" => :mavericks
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--mandir=#{man}",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_match /NetFlow/, shell_output("#{sbin}/fprobe -h").strip
  end
end
