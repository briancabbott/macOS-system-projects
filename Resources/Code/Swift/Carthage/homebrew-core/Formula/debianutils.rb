class Debianutils < Formula
  desc "Miscellaneous utilities specific to Debian"
  homepage "https://packages.debian.org/sid/debianutils"
  url "https://deb.debian.org/debian/pool/main/d/debianutils/debianutils_4.11.2.tar.xz"
  sha256 "3b680e81709b740387335fac8f8806d71611dcf60874e1a792e862e48a1650de"
  license "GPL-2.0-or-later"

  livecheck do
    url "https://packages.qa.debian.org/d/debianutils.html"
    regex(/href=.*?debianutils[._-]v?(\d+(?:.\d+)+).dsc/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "6c099d05496a2f897a037aa8b20944faa62254b5486838a29f9735c696e7cb73" => :big_sur
    sha256 "d85a9b994fc1a8feee952fcdc7f745801fb1e39fdc733ea76f69c22dd7636fef" => :arm64_big_sur
    sha256 "b6a3110aa8113eb30d7b3dd71ac194d476969322e2a184172c8da9923c497c19" => :catalina
    sha256 "5d50261564a4696a8f9d0eed99ffa0ed8eebc8344a0365d5c9b4083a54d3b6de" => :mojave
    sha256 "be68111406f254d184ffecf06a181df3000525e05b18f9b072c4cdd0ef30b3c1" => :high_sierra
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make"

    # Some commands are Debian Linux specific and we don't want them, so install specific tools
    bin.install "run-parts", "ischroot", "tempfile"
    man1.install "ischroot.1", "tempfile.1"
    man8.install "run-parts.8"
  end

  test do
    output = shell_output("#{bin}/tempfile -d #{Dir.pwd}").strip
    assert_predicate Pathname.new(output), :exist?
  end
end
