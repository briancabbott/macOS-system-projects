class Ragel < Formula
  desc "State machine compiler"
  homepage "https://www.colm.net/open-source/ragel/"
  url "https://www.colm.net/files/ragel/ragel-6.10.tar.gz"
  sha256 "5f156edb65d20b856d638dd9ee2dfb43285914d9aa2b6ec779dac0270cd56c3f"
  license "GPL-2.0"

  livecheck do
    url :homepage
    regex(/Stable.*?href=.*?ragel[._-]v?(\d+(?:\.\d+)+)\.t/im)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "712245a75110f6628e7c07130d2905577f1a533bf760692e0f4b3071df20cc40" => :big_sur
    sha256 "858ef57e50114e0406d7afc3beb7c06462bc1b5ce2155948af84af0c41d739f1" => :arm64_big_sur
    sha256 "a402204e97c35c6a9487d2b0707e27766d9b39c9c2116d49e9c561e1d0bd54b7" => :catalina
    sha256 "b9b1428abb19b6e6d8de2bccc58a059b75d7c08b38b73956bb40e764a9d0390f" => :mojave
    sha256 "8dc6d7e1a3617cd31d9738c5ae595fd57ddb157266c1970646a7d5fbba85a6ae" => :high_sierra
    sha256 "69d6d65c2ef3da7b829e3391fd17b1ef088b92c2baf64979707033e2a7dd8c01" => :sierra
    sha256 "f4ea3a8c0476fd82000223fae69170ac9f266cd36334bd60d9d6cf4fab3273c1" => :el_capitan
    sha256 "dd8469ac3e08d5d8a257ce7fc7de05de398e8521abff83eceea0741099685b38" => :yosemite
  end

  resource "pdf" do
    url "https://www.colm.net/files/ragel/ragel-guide-6.10.pdf"
    sha256 "efa9cf3163640e1340157c497db03feb4bc67d918fc34bc5b28b32e57e5d3a4e"
  end

  def install
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}"
    system "make", "install"
    doc.install resource("pdf")
  end

  test do
    testfile = testpath/"rubytest.rl"
    testfile.write <<~EOS
      %%{
        machine homebrew_test;
        main := ( 'h' @ { puts "homebrew" }
                | 't' @ { puts "test" }
                )*;
      }%%
        data = 'ht'
        %% write data;
        %% write init;
        %% write exec;
    EOS
    system bin/"ragel", "-Rs", testfile
  end
end
