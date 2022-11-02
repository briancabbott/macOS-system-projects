class XmlSecurityC < Formula
  desc "Implementation of primary security standards for XML"
  homepage "https://santuario.apache.org/"
  url "https://www.apache.org/dyn/closer.lua?path=santuario/c-library/xml-security-c-2.0.2.tar.bz2"
  mirror "https://archive.apache.org/dist/santuario/c-library/xml-security-c-2.0.2.tar.bz2"
  sha256 "39e963ab4da477b7bda058f06db37228664c68fe68902d86e334614dd06e046b"
  license "Apache-2.0"
  revision 1

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "ed512d0c411b694e5835b4b33338e9e347ceea4e564a5caeecc9e41e26b5fc53" => :big_sur
    sha256 "5ac142618a6c4f97bd5c1b554a69a9668f36e1b462910d9eaae8b8f3556fcbec" => :arm64_big_sur
    sha256 "ce0f62697cff7004fa7498ebc0dcc917206be09847847fa2ec31285b81ed04ce" => :catalina
    sha256 "eec2216263c3bb21b52418d18232034aacc69335d3e14624225627fe5364347c" => :mojave
    sha256 "5ee66d19898cd50085e90392313d3a1f45204bd111f32019251af89ee84f1ca5" => :high_sierra
    sha256 "bd1e4d4b5768f869d28850ad440e32d417f6db5d182c6049afc87575bb36ccc9" => :sierra
  end

  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"
  depends_on "xerces-c"

  def install
    ENV.cxx11

    system "./configure", "--prefix=#{prefix}", "--disable-dependency-tracking",
                          "--with-openssl=#{Formula["openssl@1.1"].opt_prefix}"
    system "make", "install"
  end

  test do
    assert_match /All tests passed/, pipe_output("#{bin}/xsec-xtest 2>&1")
  end
end
