class Openimageio < Formula
  desc "Library for reading, processing and writing images"
  homepage "https://openimageio.org/"
  url "https://github.com/OpenImageIO/oiio/archive/Release-2.2.10.1.tar.gz"
  sha256 "625f4ad666f9aa133039c0ae6ade8fc5dab835fadac988cbe65f824463d614dc"
  license "BSD-3-Clause"
  head "https://github.com/OpenImageIO/oiio.git"

  livecheck do
    url :stable
    strategy :github_latest
    regex(%r{href=.*?/tag/(?:Release[._-])?v?(\d+(?:\.\d+)+)["' >]}i)
  end

  bottle do
    sha256 "dbf0227c735a171d07aba6af381bb93671f61c79d6976163ffad13e0ce51e6eb" => :big_sur
    sha256 "2ec25e439836298b38c6f8107c788c877af223dfd02d76e39d9c18f814735ff7" => :catalina
    sha256 "bfe208f44fca47f8ed2d591f2752d7bd815c88084d6905cbb2ffdbbfd9cac909" => :mojave
  end

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "boost"
  depends_on "boost-python3"
  depends_on "ffmpeg"
  depends_on "freetype"
  depends_on "giflib"
  depends_on "ilmbase"
  depends_on "jpeg"
  depends_on "libheif"
  depends_on "libpng"
  depends_on "libraw"
  depends_on "libtiff"
  depends_on "opencolorio"
  depends_on "openexr"
  depends_on "pybind11"
  depends_on "python@3.9"
  depends_on "webp"

  # Patch to remove explicit Python framework linkage:
  # https://github.com/OpenImageIO/oiio/pull/2807
  # Remove at version bump
  patch do
    url "https://github.com/OpenImageIO/oiio/commit/5ed9d270222d18c1a789e08cea543c8cb50e1030.patch?full_index=1"
    sha256 "1fba4ce7bc33efcd1184a5aef87e76591008c0b1d823f9b82f51eae78802a591"
  end

  def install
    args = std_cmake_args + %w[
      -DCCACHE_FOUND=
      -DEMBEDPLUGINS=ON
      -DUSE_FIELD3D=OFF
      -DUSE_JPEGTURBO=OFF
      -DUSE_NUKE=OFF
      -DUSE_OPENCV=OFF
      -DUSE_OPENGL=OFF
      -DUSE_OPENJPEG=OFF
      -DUSE_PTEX=OFF
      -DUSE_QT=OFF
    ]

    # CMake picks up the system's python dylib, even if we have a brewed one.
    py3ver = Language::Python.major_minor_version Formula["python@3.9"].opt_bin/"python3"
    py3prefix = Formula["python@3.9"].opt_frameworks/"Python.framework/Versions/#{py3ver}"

    ENV["PYTHONPATH"] = lib/"python#{py3ver}/site-packages"

    args << "-DPYTHON_EXECUTABLE=#{py3prefix}/bin/python3"
    args << "-DPYTHON_LIBRARY=#{py3prefix}/lib/#{shared_library("libpython#{py3ver}")}"
    args << "-DPYTHON_INCLUDE_DIR=#{py3prefix}/include/python#{py3ver}"

    mkdir "build" do
      system "cmake", "..", *args
      system "make", "install"
    end
  end

  test do
    test_image = test_fixtures("test.jpg")
    assert_match "#{test_image} :    1 x    1, 3 channel, uint8 jpeg",
                 shell_output("#{bin}/oiiotool --info #{test_image} 2>&1")

    output = <<~EOS
      from __future__ import print_function
      import OpenImageIO
      print(OpenImageIO.VERSION_STRING)
    EOS
    assert_match version.major_minor_patch.to_s, pipe_output(Formula["python@3.9"].opt_bin/"python3", output, 0)
  end
end
