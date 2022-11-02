class XmlCoreutils < Formula
  desc "Powerful interactive system for text processing"
  homepage "https://www.lbreyer.com/xml-coreutils.html"
  url "https://www.lbreyer.com/gpl/xml-coreutils-0.8.1.tar.gz"
  sha256 "7fb26d57bb17fa770452ccd33caf288deee1d757a0e0a484b90c109610d1b7df"
  license "GPL-3.0"

  bottle do
    cellar :any
    rebuild 1
    sha256 "6e5400968229c313cab973cffdbb77b88c30a5301066626b34b96e0a46578fc8" => :big_sur
    sha256 "7094a5673f2ab6ba2fa45c587397650f4d9b2ccea1ab66925f58ef776683298d" => :arm64_big_sur
    sha256 "e098f5b2d9af801bb12c65044668091b175dcca43cec7251acb0d3e1ccad4fed" => :catalina
    sha256 "9be4dcb20fd773296a26df8495c5097b273a2a0d89f6abc1545a713ba94e1b07" => :mojave
    sha256 "83023841339cb02ad53de64e30aa0c491e4acd4ae4602bd84847aca42ac02e00" => :high_sierra
    sha256 "5f7519c9be40f731b0dca6238b3bedf4070f0663fc47ab8e4b0eff02d187718c" => :sierra
    sha256 "19bdcacd49657e78f82fd7743a50266ff4945e644b069ac2c39a8787a57911a5" => :el_capitan
    sha256 "1342c807e5ddc23a72e750f07258864fdf2fc1a8ce9072cb7797955fdd0e3656" => :yosemite
    sha256 "6bada2b9690d698b77f9293cf7f1066e77d36c8c07ba2892b2869ddb0516bc6d" => :mavericks
  end

  depends_on "s-lang"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.xml").write <<~EOS
      <hello>world!</hello>
    EOS
    assert_match /0\s+1\s+1/, shell_output("#{bin}/xml-wc test.xml")
  end
end
