class Kanif < Formula
  desc "Cluster management and administration tool"
  homepage "http://taktuk.gforge.inria.fr/kanif/"
  url "https://gforge.inria.fr/frs/download.php/26773/kanif-1.2.2.tar.gz"
  sha256 "3f0c549428dfe88457c1db293cfac2a22b203f872904c3abf372651ac12e5879"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "ea9c4a641227762b26e89a7015aa328d16fdfdb23796c29abcdb83ab19638b59" => :big_sur
    sha256 "cfc06314d243173b2b0f0de1188570adde896ef6002dcbb75e7ce9fe056ae172" => :arm64_big_sur
    sha256 "e69f751ab52a8f0892e452bcd3e2e43df27a94cd5e3a5b8cc972529758e0ebf0" => :catalina
    sha256 "4de62a3ee570ed87d4a6c5ec559e901323bfc333e67714b618f9bafc9eb6b6e9" => :mojave
    sha256 "adcf3a42f6db8afb8011bbc0e5bcae234ad1d218b0f3b9a0ea884ff6499ab4f0" => :high_sierra
    sha256 "8a89062ef7794743c32bc611c1969f42941af573c8dd9973abee7d4255293df1" => :sierra
    sha256 "ebe67d82aa8745a8ff29a82b9520e0fb4892a9645e17a12e8aeae4fda96cc8b4" => :el_capitan
    sha256 "7fc9517feb23867afe77730cdf28bb386570ccb5368d3b0c6b396214abf07e69" => :yosemite
    sha256 "3c003b7621fdef7d8451c3bab928e5b7bb0db0ff1fbe2c5d83b160a021ef93e6" => :mavericks
  end

  depends_on "taktuk"

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_match "taktuk -s -c 'ssh' -l brew",
      shell_output("#{bin}/kash -q -l brew -r ssh")
  end
end
