class Utf8proc < Formula
  desc "Clean C library for processing UTF-8 Unicode data"
  homepage "https://juliastrings.github.io/utf8proc/"
  url "https://github.com/JuliaStrings/utf8proc/archive/v2.6.1.tar.gz"
  sha256 "4c06a9dc4017e8a2438ef80ee371d45868bda2237a98b26554de7a95406b283b"
  license all_of: ["MIT", "Unicode-DFS-2015"]

  bottle do
    cellar :any
    sha256 "7d57bb4f93fdfd377fda65e74b9bf1cb7b2b4875a4481682a4b2a39cfe65d3e7" => :big_sur
    sha256 "e3845f14873623bba6259adc3f2b129d7a2ca41683764de6335eda07a30c3de7" => :arm64_big_sur
    sha256 "cf77fdee400e5692c3b78f0fc599e575b071b11a70ff1f3443e22c8dd14bd0fe" => :catalina
    sha256 "1bee383abb84a7f9921e1c6b3aa26de0c8c79295c4ae184959521bf537ba4552" => :mojave
  end

  def install
    system "make", "install", "prefix=#{prefix}"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <string.h>
      #include <utf8proc.h>

      int main() {
        const char *version = utf8proc_version();
        return strnlen(version, sizeof("1.3.1-dev")) > 0 ? 0 : -1;
      }
    EOS

    system ENV.cc, "test.c", "-I#{include}", "-L#{lib}", "-lutf8proc", "-o", "test"
    system "./test"
  end
end
