class Clzip < Formula
  desc "C language version of lzip"
  homepage "https://www.nongnu.org/lzip/clzip.html"
  url "https://download.savannah.gnu.org/releases/lzip/clzip/clzip-1.11.tar.gz"
  sha256 "d9d51212afa80371dc2546d278ef8ebbb3cd57c06fdd761b7b204497586d24c0"
  license "GPL-2.0"

  livecheck do
    url "https://download.savannah.gnu.org/releases/lzip/clzip/"
    regex(/href=.*?clzip[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "07f5030dd16ca3aaa2eeca40eb0320c75ab7144962c071cac2e6db4354e009ed" => :big_sur
    sha256 "4d8f40d897f2ed119e304bf3835f0721d4e843c6818f41755f91bde7491837fd" => :arm64_big_sur
    sha256 "ae164ce5fd5a020988dc4e177a25cf19f924b282e561cdabf57577c88940ac2b" => :catalina
    sha256 "654a41ff519d4109d38f17c3fb321f130e60c9d72d137674f2dde9ef5cf129be" => :mojave
    sha256 "2a3bf6819a2fdbef49fa7bf1e1cea7ef6c6d090bf8fa787fe7b2a582b2631045" => :high_sierra
    sha256 "26dbdb3a397aa3f62acc15bbbf599a32e5b832564ea6ddc6e15327baac90b5ba" => :sierra
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
    pkgshare.install "testsuite"
  end

  test do
    cp_r pkgshare/"testsuite", testpath
    cd "testsuite" do
      ln_s bin/"clzip", "clzip"
      system "./check.sh"
    end
  end
end
