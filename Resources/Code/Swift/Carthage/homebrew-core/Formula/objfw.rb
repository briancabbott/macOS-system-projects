class Objfw < Formula
  desc "Portable, lightweight framework for the Objective-C language"
  homepage "https://heap.zone/objfw/"
  url "https://heap.zone/objfw/downloads/objfw-0.90.2.tar.gz"
  sha256 "4de24703d45638093a5196eba278a05b3643e8be0ae2eece5c81ba3e2c20bdbb"

  bottle do
    sha256 "d0ef5bb1b837ab3ebebc3737361bd65f739d9592f8e0b139a7786a19df8108c9" => :big_sur
    sha256 "59dd4798a6017b6062614f70004c87feb3bbf7aecb9ac2165b5b6f92353c6361" => :arm64_big_sur
    sha256 "07ede29c16dec01d6eb8dd2dca6f79ec993f99298b38a78276f6257ebfb71c6a" => :catalina
    sha256 "abc09195b6abf66d1d638af2999abe712a41cdcbb4bbf8d7ea422443150ae637" => :mojave
    sha256 "33c72d86bb5a56ff4a2c9607707edb31f7af21bf863c8d34d95f6c527d9ee483" => :high_sierra
    sha256 "2369c4233bafe95aeea87f678cc5e0f0b001d36b5aeff6a7b6512f766d77eb5e" => :sierra
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/objfw-new", "app", "Test"
    system "#{bin}/objfw-compile", "-o", "t", "Test.m"
    system "./t"
  end
end
