class Libosmium < Formula
  desc "Fast and flexible C++ library for working with OpenStreetMap data"
  homepage "https://osmcode.org/libosmium/"
  url "https://github.com/osmcode/libosmium/archive/v2.16.0.tar.gz"
  sha256 "42bbef97226d7db7ce3eeb474603e5b1f2f0f86cec85498868e9416e7cdf5bd5"
  license "BSL-1.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "920817014e8e37a9586ef09d17a3ea110e8bfa4615354b680e7a228a7169f5fa" => :big_sur
    sha256 "536aed752034cfa5f2d3f1bf945872ece082afd3179b3403ea5aa649d7493c06" => :catalina
    sha256 "2e053a4724e720c1355409fd2d3accaef046b9755d92f3bcfaf1dd7897739e24" => :mojave
  end

  depends_on "boost" => :build
  depends_on "cmake" => :build

  uses_from_macos "expat"

  resource "protozero" do
    url "https://github.com/mapbox/protozero/archive/v1.7.0.tar.gz"
    sha256 "beffbdfab060854fd770178a8db9c028b5b6ee4a059a2fed82c46390a85f3f31"
  end

  def install
    resource("protozero").stage { libexec.install "include" }
    system "cmake", ".", "-DINSTALL_GDALCPP=ON",
                         "-DINSTALL_UTFCPP=ON",
                         "-DPROTOZERO_INCLUDE_DIR=#{libexec}/include",
                         *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.osm").write <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <osm version="0.6" generator="handwritten">
        <node id="1" lat="0.001" lon="0.001" user="Dummy User" uid="1" version="1" changeset="1" timestamp="2015-11-01T19:00:00Z"></node>
        <node id="2" lat="0.002" lon="0.002" user="Dummy User" uid="1" version="1" changeset="1" timestamp="2015-11-01T19:00:00Z"></node>
        <way id="1" user="Dummy User" uid="1" version="1" changeset="1" timestamp="2015-11-01T19:00:00Z">
          <nd ref="1"/>
          <nd ref="2"/>
          <tag k="name" v="line"/>
        </way>
        <relation id="1" user="Dummy User" uid="1" version="1" changeset="1" timestamp="2015-11-01T19:00:00Z">
          <member type="node" ref="1" role=""/>
          <member type="way" ref="1" role=""/>
        </relation>
      </osm>
    EOS

    (testpath/"test.cpp").write <<~EOS
      #include <cstdlib>
      #include <iostream>
      #include <osmium/io/xml_input.hpp>

      int main(int argc, char* argv[]) {
        osmium::io::File input_file{argv[1]};
        osmium::io::Reader reader{input_file};
        while (osmium::memory::Buffer buffer = reader.read()) {}
        reader.close();
      }
    EOS

    system ENV.cxx, "-std=c++11", "-stdlib=libc++", "-lexpat", "-o", "libosmium_read", "test.cpp"
    system "./libosmium_read", "test.osm"
  end
end
