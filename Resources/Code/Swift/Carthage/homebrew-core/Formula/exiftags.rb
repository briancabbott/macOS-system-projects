class Exiftags < Formula
  desc "Utility to read EXIF tags from a digital camera JPEG file"
  homepage "https://johnst.org/sw/exiftags/"
  url "https://johnst.org/sw/exiftags/exiftags-1.01.tar.gz"
  sha256 "d95744de5f609f1562045f1c2aae610e8f694a4c9042897a51a22f0f0d7591a4"

  bottle do
    cellar :any_skip_relocation
    sha256 "e6f10871b2577320dd9c219faa1e1b31fac8a311cde68810d233aaafa79a5a08" => :big_sur
    sha256 "2229163e3493c0fae8cf83479f5c5258b926437b353038cd738d7f7d50fc20f9" => :arm64_big_sur
    sha256 "f4236ab5e0f9f3710e32ca5a6932f47d0b11c232c6f84bfc4ac4694fb26ac832" => :catalina
    sha256 "8287c22dcfeaaf6a28a4036ce26c4a1febfe8a3bc01a1d7320b667d56d0b2e43" => :mojave
    sha256 "1ba9c96bf8630f50faf8bb5045bace46c5c24962d439a496a6f606b7bc886a08" => :high_sierra
    sha256 "7aaa2a8e78b03e4f842c84a46ce7fb5ed8ff1a956ababde1f26bc716431a67e0" => :sierra
    sha256 "47d75e83f89d0db4a54d779d9c9820fbb788c102738824e86b83a441d9a60af8" => :el_capitan
    sha256 "23a94f2c2694d52ef393e751e23a01c4ed23c0ca7004b6597546047310e73f53" => :yosemite
    sha256 "2ca339b45b3ea518ca5b39262b4c68cc54187a2bfca7d7a52eded5685c81b3c9" => :mavericks
  end

  def install
    bin.mkpath
    man1.mkpath
    system "make", "prefix=#{prefix}", "mandir=#{man}", "install"
  end

  test do
    test_image = test_fixtures("test.jpg")
    assert_match "couldn't find Exif data",
                 shell_output("#{bin}/exiftags #{test_image} 2>&1", 1)
  end
end
