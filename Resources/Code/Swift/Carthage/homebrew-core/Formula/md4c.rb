class Md4c < Formula
  desc "C Markdown parser. Fast. SAX-like interface"
  homepage "https://github.com/mity/md4c"
  url "https://github.com/mity/md4c/archive/release-0.4.7.tar.gz"
  sha256 "f1b12d7aeb64fcbc7092c832e1a8b137102fec168961c87222fa599aedc19035"
  license "MIT"

  bottle do
    cellar :any
    sha256 "5253c9afbcee9804cd582fd860b4d5f8b9805d9a30ea97da59f4c8332738e5db" => :big_sur
    sha256 "51a8a4e90c02e9f1431252de17ea0a6af8b520659c87b8c6e4f68fc48c231085" => :catalina
    sha256 "de32cd227ac8eb3e44c7f5d1763959f2ac26200444f8023de9a343f40bdb341b" => :mojave
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    # test md2html
    (testpath/"test_md.md").write <<~EOS
      # Title
      some text
    EOS
    system bin/"md2html", "./test_md.md"

    # test libmd4c
    (testpath/"test_program.c").write <<~EOS
      #include <stddef.h>
      #include <md4c.h>

      MD_CHAR* text = "# Title\\nsome text";

      int test_block(MD_BLOCKTYPE type, void* detail, void* data) { return 0; }
      int test_span(MD_SPANTYPE type, void* detail, void* data) { return 0; }
      int test_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) { return 0; }
      int main() {
        MD_PARSER parser = {
          .enter_block = test_block,
          .leave_block = test_block,
          .enter_span = test_span,
          .leave_span = test_span,
          .text = test_text
        };
        int result = md_parse(text, sizeof(text), &parser, NULL);
        return result;
      }
    EOS
    system ENV.cc, "test_program.c", "-L#{lib}", "-lmd4c", "-o", "test_program"
    system "./test_program"
  end
end
