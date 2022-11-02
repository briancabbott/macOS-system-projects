class OrocosKdl < Formula
  desc "Orocos Kinematics and Dynamics C++ library"
  homepage "https://orocos.org/"
  url "https://github.com/orocos/orocos_kinematics_dynamics/archive/v1.4.0.tar.gz"
  sha256 "05b93e759923684dc07433ccae1e476d158d89b3c2be5079c20062406da7b4dd"
  license "LGPL-2.1"

  bottle do
    cellar :any
    sha256 "11f44349bb30654acb195b84796e5b9492e5fa7539c1c8aababf98eaea239cf2" => :big_sur
    sha256 "a935a1f650fdff2a93909d71bca9cf37a907c626ab08f18f5479323fb32b46bf" => :arm64_big_sur
    sha256 "e65054479e7b34fd559f72f4485a5e34b57b457325bb8487576309ddad7a26fa" => :catalina
    sha256 "3d88fc55d86c9d1194ed3896bf1524405997e601ae75acbd37b176f882f07868" => :mojave
    sha256 "32e9cd3e10a20c046a45122557dda364352619c59acffca07f8c858cdcff9765" => :high_sierra
    sha256 "2696ca8480d6be3de18a141630388ef5fe5486096c02f99726b6d07cc91ff958" => :sierra
    sha256 "87d3407e88f69187f10119d109321c8ece7c04154262475665f462923f69ffe9" => :el_capitan
  end

  depends_on "cmake" => :build
  depends_on "eigen"

  def install
    cd "orocos_kdl" do
      system "cmake", ".", "-DEIGEN3_INCLUDE_DIR=#{Formula["eigen"].opt_include}/eigen3",
                           *std_cmake_args
      system "make", "install"
    end
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <kdl/frames.hpp>
      int main()
      {
        using namespace KDL;
        Vector v1(1.,0.,1.);
        Vector v2(1.,0.,1.);
        assert(v1==v2);
        return 0;
      }
    EOS
    system ENV.cxx, "test.cpp", "-I#{include}", "-L#{lib}", "-lorocos-kdl",
                    "-o", "test"
    system "./test"
  end
end
