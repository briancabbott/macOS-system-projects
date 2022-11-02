class Xqilla < Formula
  desc "XQuery and XPath 2 command-line interpreter"
  homepage "https://xqilla.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/xqilla/XQilla-2.3.4.tar.gz"
  sha256 "292631791631fe2e7eb9727377335063a48f12611d641d0296697e0c075902eb"

  livecheck do
    url :stable
    regex(%r{url=.*?/XQilla[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "ac66706739f52be905422e387435524387fdec6ca86243aad5b8be446182d59a" => :big_sur
    sha256 "1b8493188f6fc779948193c1ae7cc803e85a4a18c32464c039448a27f830d9fe" => :arm64_big_sur
    sha256 "3e01ca81220688c9680e3c23c0f7434f415e2b1e7b2e812f514a540eb51b50cd" => :catalina
    sha256 "93ae09129c45ee7b1a4ecfe996c305791e06833c1e73b604b33282e5ea90248a" => :mojave
    sha256 "38579e6ab1b6f6801ca5404cc79fcd972f395b9dd2e981672889b3eac5441c86" => :high_sierra
    sha256 "0f1ef8f2aa1349b723062426a3e44fba2821bcf93316bacabf4c5e2948093bc4" => :sierra
    sha256 "4326ec876d3e05647320c4ab55824c37531af997cc723f303fac4c4b40153753" => :el_capitan
  end

  depends_on "xerces-c"

  conflicts_with "zorba", because: "both supply `xqc.h`"

  def install
    ENV.cxx11

    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--with-xerces=#{HOMEBREW_PREFIX}",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <iostream>
      #include <xqilla/xqilla-simple.hpp>

      int main(int argc, char *argv[]) {
        XQilla xqilla;
        AutoDelete<XQQuery> query(xqilla.parse(X("1 to 100")));
        AutoDelete<DynamicContext> context(query->createDynamicContext());
        Result result = query->execute(context);
        Item::Ptr item;
        while(item == result->next(context)) {
          std::cout << UTF8(item->asString(context)) << std::endl;
        }
        return 0;
      }
    EOS
    system ENV.cxx, "-I#{include}", "-L#{lib}", "-lxqilla",
           "-I#{Formula["xerces-c"].opt_include}",
           "-L#{Formula["xerces-c"].opt_lib}", "-lxerces-c",
           testpath/"test.cpp", "-o", testpath/"test"
    system testpath/"test"
  end
end
