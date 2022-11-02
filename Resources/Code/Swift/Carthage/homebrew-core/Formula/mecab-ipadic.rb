class MecabIpadic < Formula
  desc "IPA dictionary compiled for MeCab"
  homepage "https://taku910.github.io/mecab/"
  # Canonical url is https://drive.google.com/uc?export=download&id=0B4y35FiV1wh7MWVlSDBCSXZMTXM
  url "https://deb.debian.org/debian/pool/main/m/mecab-ipadic/mecab-ipadic_2.7.0-20070801+main.orig.tar.gz"
  version "2.7.0-20070801"
  sha256 "b62f527d881c504576baed9c6ef6561554658b175ce6ae0096a60307e49e3523"

  # We check the Debian index page because the first-party website uses a Google
  # Drive download URL and doesn't list the version in any other way, so we
  # can't identify the newest version there.
  livecheck do
    url "https://deb.debian.org/debian/pool/main/m/mecab-ipadic/"
    regex(/href=.*?mecab-ipadic[._-]v?(\d+(?:\.\d+)+(?:-\d+)?)(?:\+main)?\.orig\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "4fc2878d95314057c5d0f726cc1dacf2ce110c7e84b77806e958970f9b34ccc5" => :big_sur
    sha256 "bdd2a69bbcbfe6e051278c94e4e19c6bfde63e2a3e525e2c57da0afb37ee5b6f" => :arm64_big_sur
    sha256 "90271975d35925136a14f2563e4b5201bed51b5c1fc27249d916676027c1016e" => :catalina
    sha256 "30967b4167d34f05c79f185d71a40198fff4067d0cce82aed59383548c898681" => :mojave
    sha256 "ef5cf167b05fd74457d5c31a46750450e8f80720ebc705766ee10df6ed41a861" => :high_sierra
    sha256 "33f42c18d7347708a56d8846c0bde5c8291b7685ce06b342e96442bca35f6663" => :sierra
    sha256 "9f0ae0a62141e3b28807349cb7a9560e36770acb869f4a4e7a54ea1a28ef8ba5" => :el_capitan
    sha256 "55703c812de3e7cff503b9cd1eafa0656b3f17c4885165ce4d8e4d2b2356050e" => :yosemite
    sha256 "0a9ea36b7cc03f73ae34f72e078b7e84ebe814cf8e1cfbea2d5f876c1893b1c5" => :mavericks
  end

  depends_on "mecab"

  link_overwrite "lib/mecab/dic"

  def install
    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --prefix=#{prefix}
      --with-charset=utf8
      --with-dicdir=#{lib}/mecab/dic/ipadic
    ]

    system "./configure", *args
    system "make", "install"
  end

  def caveats
    <<~EOS
      To enable mecab-ipadic dictionary, add to #{HOMEBREW_PREFIX}/etc/mecabrc:
        dicdir = #{HOMEBREW_PREFIX}/lib/mecab/dic/ipadic
    EOS
  end

  test do
    (testpath/"mecabrc").write <<~EOS
      dicdir = #{HOMEBREW_PREFIX}/lib/mecab/dic/ipadic
    EOS

    pipe_output("mecab --rcfile=#{testpath}/mecabrc", "すもももももももものうち\n", 0)
  end
end
