class Minicom < Formula
  desc "Menu-driven communications program"
  homepage "https://packages.debian.org/sid/minicom"
  url "https://deb.debian.org/debian/pool/main/m/minicom/minicom_2.8.orig.tar.bz2"
  sha256 "38cea30913a20349326ff3f1763ee1512b7b41601c24f065f365e18e9db0beba"
  license "GPL-2.0-or-later"

  livecheck do
    url "https://deb.debian.org/debian/pool/main/m/minicom/"
    regex(/href=.*?minicom[._-]v?(\d+(?:\.\d+)+)\.orig\.t/i)
  end

  bottle do
    sha256 "ac0a7c58888a3eeb78bbc24d8a47fa707d7e3761c4b28f46527434d49e254b55" => :big_sur
    sha256 "396aa4bed62d6a9162d061ff1b97a1c5fe25e5a890141d4f39c1849564e3521f" => :arm64_big_sur
    sha256 "9cee8e5839a3e19aa732307ee70246b1567ddc3a643ef39aa91b6d888301f6e5" => :catalina
    sha256 "e2b702dec206101120ce947ca2a999c9f5fe7e8c62f95b65091146b865acb268" => :mojave
  end

  def install
    # There is a silly bug in the Makefile where it forgets to link to iconv. Workaround below.
    ENV["LIBS"] = "-liconv"

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"

    (prefix/"etc").mkdir
    (prefix/"var").mkdir
    (prefix/"etc/minirc.dfl").write "pu lock #{prefix}/var\npu escape-key Escape (Meta)\n"
  end

  def caveats
    <<~EOS
      Terminal Compatibility
      ======================
      If minicom doesn't see the LANG variable, it will try to fallback to
      make the layout more compatible, but uglier. Certain unsupported
      encodings will completely render the UI useless, so if the UI looks
      strange, try setting the following environment variable:

        LANG="en_US.UTF-8"

      Text Input Not Working
      ======================
      Most development boards require Serial port setup -> Hardware Flow
      Control to be set to "No" to input text.
    EOS
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/minicom -v", 1)
  end
end
