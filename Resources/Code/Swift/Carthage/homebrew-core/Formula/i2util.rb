class I2util < Formula
  desc "Internet2 utility tools"
  homepage "https://software.internet2.edu/"
  url "https://software.internet2.edu/sources/I2util/I2util-1.2.tar.gz"
  sha256 "3b704cdb88e83f7123f3cec0fe3283b0681cc9f80c426c3f761a0eefd1d72c59"

  livecheck do
    # HTTP allows directory listing while HTTPS returns 403
    url "http://software.internet2.edu/sources/I2util/"
    regex(/href=.*?I2util[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "f3e907bcbbb2fb450d4566e5f3fd5e1481d16a6e1008142e13b8c0f3fa396b56" => :big_sur
    sha256 "15ccc5cf4852a0d03f022a6127de5ef518a162e8e9cd69ec3800a8c9a42e2c1b" => :arm64_big_sur
    sha256 "583442b07b8d0007ad6b3302daefd4bc5d2ce0b71ed3bc7f73c68eb3fb3e3fdd" => :catalina
    sha256 "39d1540d90f798d79b38844fe234329513548c6882204fb69c1b5f372d1f7c5e" => :mojave
    sha256 "47c66cf5e0bfec05a5c254dc4088fe2ec3dd45772d729bd0b38146afdfbd0f0a" => :high_sierra
    sha256 "562e2d9021ff8044ca05a63c31d6560e5071ffc62f34ff1046cf195118b3471a" => :sierra
    sha256 "44f87d48502ae3e34ebfc0882aa689a70e8c92d398247c5a53e2f4b7d7652b39" => :el_capitan
    sha256 "ad1821b2637c75638de2ecd2bd3127a0c8300fe4fbd72c18ae648a131b97b6f7" => :yosemite
    sha256 "b9a22dff1f4a26be02712d17de832a23fc3dbe5eee75ab62b72ffb5b18ecbd99" => :mavericks
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <I2util/util.h>
      #include <string.h>
      int main() {
        uint8_t buf[2];
        if (!I2HexDecode("beef", buf, sizeof(buf))) return 1;
        if (buf[0] != 190 || buf[1] != 239) return 1;
        return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-lI2util", "-o", "test"
    system "./test"
  end
end
