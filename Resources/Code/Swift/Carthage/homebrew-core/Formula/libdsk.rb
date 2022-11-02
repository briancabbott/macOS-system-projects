class Libdsk < Formula
  desc "Library for accessing discs and disc image files"
  homepage "https://www.seasip.info/Unix/LibDsk/"
  url "https://www.seasip.info/Unix/LibDsk/libdsk-1.4.2.tar.gz"
  sha256 "71eda9d0e33ab580cea1bb507467877d33d887cea6ec042b8d969004db89901a"

  livecheck do
    url :homepage
    regex(/Stable version.*?href=.*?libdsk[._-]v?(\d+(?:\.\d+)+)\.t/im)
  end

  bottle do
    sha256 "19a28a828ba163c5bbb988cfc22e5c0a4d7a7c1f6e9cd479323c345e2175c017" => :big_sur
    sha256 "2804cbad27cb5d942cac8be6682bc7da39f7a4c122b6f79d1e3ee58aaaea0a0b" => :arm64_big_sur
    sha256 "f444a8f81a4767668f4cbffa2ef09268279d23780e92b7d4bc2d6ed44c9cd675" => :catalina
    sha256 "47485db7001965531b700308a3d464a616703ddd8fdca64c8a7d2b5049481eb5" => :mojave
    sha256 "b4fa361c1800fd348c804873fd03f8663f7324eed228c3ba2e2d809a58fbbb97" => :high_sierra
    sha256 "d46bdf8e9c779b22a2a21c123572c08130aa36b8a817365ee3bd76219478aad3" => :sierra
    sha256 "b14fb001603c2ba33a26c0f49c7b008659ca5aa05ffaa01ab8147bac4da40d46" => :el_capitan
  end

  def install
    # Avoid lyx dependency
    inreplace "Makefile.in", "SUBDIRS = . include lib tools man doc",
                             "SUBDIRS = . include lib tools man"

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make"
    system "make", "check"
    system "make", "install"
    doc.install Dir["doc/*.{html,pdf,sample,txt}"]
  end

  test do
    assert_equal "#{name} version #{version}\n", shell_output(bin/"dskutil --version")
  end
end
