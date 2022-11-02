class XmlToolingC < Formula
  desc "Provides a higher level interface to XML processing"
  homepage "https://wiki.shibboleth.net/confluence/display/OpenSAML/XMLTooling-C"
  url "https://shibboleth.net/downloads/c++-opensaml/3.2.0/xmltooling-3.2.0.tar.bz2"
  sha256 "635ce0e912d8fbd450103c274237067923efac3e1b3662b4d3040f3ac5eb2e86"
  license "Apache-2.0"

  livecheck do
    url "https://shibboleth.net/downloads/c++-opensaml/latest/"
    regex(/href=.*?xmltooling[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "57c8c16990f589f0e07a7e5d57dd202c4f35b6e66d57bbda66d4d9bc2af6bd33" => :big_sur
    sha256 "65e021c1f203021118f1ed17a67869077a2ae014774729173010c8095e3b89ec" => :arm64_big_sur
    sha256 "859a056b4271610e876b42606d145a0ddc2d79cb94c0470e2ca93cdef38c4e2b" => :catalina
    sha256 "69d6679f8c610867e03269af38ce56306af656a2e1f7b3bbce30d25085d6ae9a" => :mojave
  end

  depends_on "pkg-config" => :build
  depends_on "boost"
  depends_on "log4shib"
  depends_on "openssl@1.1"
  depends_on "xerces-c"
  depends_on "xml-security-c"

  uses_from_macos "curl"

  def install
    ENV.cxx11

    ENV.prepend_path "PKG_CONFIG_PATH", "#{Formula["openssl@1.1"].opt_lib}/pkgconfig"

    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
