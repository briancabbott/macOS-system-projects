class Lasi < Formula
  desc "C++ stream output interface for creating Postscript documents"
  homepage "https://www.unifont.org/lasi/"
  url "https://downloads.sourceforge.net/project/lasi/lasi/1.1.3%20Source/libLASi-1.1.3.tar.gz"
  sha256 "5e5d2306f7d5a275949fb8f15e6d79087371e2a1caa0d8f00585029d1b47ba3b"
  license "GPL-2.0-or-later"
  revision 2
  head "https://svn.code.sf.net/p/lasi/code/trunk"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 2
    sha256 "d4d9a1f05e4acef822930f62b4dd5b5f87f815e01523eb41b91df079af35b69b" => :big_sur
    sha256 "f6f4ac7da7af9beba184fff05fd4419335c07710beb3a2e3646afdde31745770" => :arm64_big_sur
    sha256 "9c9b3d4df3fef9c27ccc60f51583976cfb7093c5ea345c0dced428e0539b7ede" => :catalina
    sha256 "95eed6a78b95300f4b496bdba60b0542c9b66e5ce96ca7c8fcd081e76eebc675" => :mojave
  end

  depends_on "cmake" => :build
  depends_on "doxygen" => :build
  depends_on "pkg-config" => :build
  depends_on "pango"

  def install
    args = std_cmake_args.dup

    # std_cmake_args tries to set CMAKE_INSTALL_LIBDIR to a prefix-relative
    # directory, but lasi's cmake scripts don't like that
    args.map! { |x| x.start_with?("-DCMAKE_INSTALL_LIBDIR=") ? "-DCMAKE_INSTALL_LIBDIR=#{lib}" : x }

    # If we build/install examples they result in shim/cellar paths in the
    # installed files.  Instead we don't build them at all.
    inreplace "CMakeLists.txt", "add_subdirectory(examples)", ""

    system "cmake", ".", *args

    system "make", "install"
  end
end
