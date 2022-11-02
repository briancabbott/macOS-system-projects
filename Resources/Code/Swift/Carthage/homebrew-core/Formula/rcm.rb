class Rcm < Formula
  desc "RC file (dotfile) management"
  homepage "https://thoughtbot.github.io/rcm/rcm.7.html"
  url "https://thoughtbot.github.io/rcm/dist/rcm-1.3.4.tar.gz"
  sha256 "9b11ae37449cf4d234ec6d1348479bfed3253daba11f7e9e774059865b66c24a"
  license "BSD-3-Clause"

  # The first-party website doesn't appear to provide links to archive files, so
  # we check the Git repository tags instead.
  livecheck do
    url "https://github.com/thoughtbot/rcm.git"
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "1ae14043eb53ab02db26a3bf33d15d817a09917788f0165bbcc538f77a9d38fd" => :big_sur
    sha256 "a43a7792728bc4c441e997bc6e0879aecc237d1c95c7a47ff49093e33ad14979" => :arm64_big_sur
    sha256 "86ac10a7254567afb24c9816f6a80dd90a81bc8cd8619c112e59c0950929ef14" => :catalina
    sha256 "44c9524d9d5ce8ea5310fe6681b040d6c685cec693446f617686f82929d83c6b" => :mojave
    sha256 "7130060f9a26eda6a704eb06bda4c04a4cc0b0980f1c9d3fc5dce876fa5a3fdf" => :high_sierra
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/".gitconfig").write <<~EOS
      [user]
      	name = Test User
      	email = test@test.com
    EOS
    assert_match /(Moving|Linking)\.\.\./x, shell_output("#{bin}/mkrc -v ~/.gitconfig")
  end
end
