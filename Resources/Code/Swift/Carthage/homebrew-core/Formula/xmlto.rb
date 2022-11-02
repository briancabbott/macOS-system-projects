class Xmlto < Formula
  desc "Convert XML to another format (based on XSL or other tools)"
  homepage "https://pagure.io/xmlto/"
  url "https://releases.pagure.org/xmlto/xmlto-0.0.28.tar.bz2"
  sha256 "1130df3a7957eb9f6f0d29e4aa1c75732a7dfb6d639be013859b5c7ec5421276"
  license "GPL-2.0-or-later"

  livecheck do
    url "https://releases.pagure.org/xmlto/?C=M&O=D"
    regex(/href=.*?xmlto[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 2
    sha256 "95502b71000319da58971ea17c0dab0f326d20ce4c09d074fe4c7fe89c66d002" => :big_sur
    sha256 "bec2a4a7797f07fc73a636a40e85cb0504da4c5e34328456c28e78c84ce95324" => :arm64_big_sur
    sha256 "d2c21b9b398191e21dcf6e7ac53e4dd46fb59d29173e4d8443ac296101cce58f" => :catalina
    sha256 "8fca3be2271ae8e7fb646b011969ba4030f7421118a4ea6b11eca1ac0fe6979b" => :mojave
    sha256 "1214da1d14a8f01d8b8d0ead6606207ff5a29fb7ab104d6af47e57fbca4ffcc7" => :high_sierra
  end

  depends_on "docbook"
  depends_on "docbook-xsl"
  # Doesn't strictly depend on GNU getopt, but macOS system getopt(1)
  # does not support longopts in the optstring, so use GNU getopt.
  depends_on "gnu-getopt"

  uses_from_macos "libxslt"

  # xmlto forces --nonet on xsltproc, which causes it to fail when
  # DTDs/entities aren't available locally.
  patch :DATA

  def install
    # GNU getopt is keg-only, so point configure to it
    ENV["GETOPT"] = Formula["gnu-getopt"].opt_bin/"getopt"
    # Prevent reference to Homebrew shim
    ENV["SED"] = "/usr/bin/sed"
    # Find our docbook catalog
    ENV["XML_CATALOG_FILES"] = "#{etc}/xml/catalog"

    ENV.deparallelize
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test").write <<~EOS
      <?xmlif if foo='bar'?>
      Passing test.
      <?xmlif fi?>
    EOS
    assert_equal "Passing test.", shell_output("cat test | #{bin}/xmlif foo=bar").strip
  end
end


__END__
--- xmlto-0.0.25/xmlto.in.orig
+++ xmlto-0.0.25/xmlto.in
@@ -209,7 +209,7 @@
 export VERBOSE
 
 # Disable network entities
-XSLTOPTS="$XSLTOPTS --nonet"
+#XSLTOPTS="$XSLTOPTS --nonet"
 
 # The names parameter for the XSLT stylesheet
 XSLTPARAMS=""
