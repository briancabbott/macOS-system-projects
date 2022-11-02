class Osmfilter < Formula
  desc "Command-line tool to filter OpenStreetMap files for specific tags"
  homepage "https://wiki.openstreetmap.org/wiki/Osmfilter"
  url "https://gitlab.com/osm-c-tools/osmctools.git",
      tag:      "0.9",
      revision: "f341f5f237737594c1b024338f0a2fc04fabdff3"
  license "AGPL-3.0"
  head "https://gitlab.com/osm-c-tools/osmctools.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "5647d8f3a704bd126e2b5f24237febb50989798b425147baf1d1ce1a08fbdaaa" => :big_sur
    sha256 "4b37db3c9ebe77673bbd83fb7d2e6c215760450987df2ded64044eccf6f34d3b" => :arm64_big_sur
    sha256 "5e2b755a970b7432fb076d787cb1777df18861832d0e4d45132fd84e4d7aea20" => :catalina
    sha256 "470532603de299b9073f5511b8be798558d430f86ba4f37b330a497ec9fdae48" => :mojave
    sha256 "b2e2d4190462b0b0e473da4a50ab5e25da007aca21db898d2d359e9e9eb2cde7" => :high_sierra
    sha256 "d7a8285fe18af71d0093b89e9b5613a4fe30ceb4978e07f61ad1974e734d7f50" => :sierra
    sha256 "6a0fd608e0bc8094f08edb6f86a51b45745506d3ef84e0454ef1498dd77f61b0" => :el_capitan
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build

  uses_from_macos "zlib"

  resource "pbf" do
    url "https://download.gisgraphy.com/openstreetmap/pbf/AD.tar.bz2"
    sha256 "f8decd915758139e8bff2fdae6102efa0dc695b9d1d64cc89a090a91576efda9"
  end

  def install
    system "autoreconf", "-v", "-i"
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    resource("pbf").stage do
      system bin/"osmconvert", "AD", "-o=test.o5m"
      system bin/"osmfilter", "test.o5m",
        "--drop-relations", "--drop-ways", "--drop-nodes"
    end
  end
end
