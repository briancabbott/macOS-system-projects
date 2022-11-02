class Mktorrent < Formula
  desc "Create BitTorrent metainfo files"
  homepage "https://github.com/Rudde/mktorrent/wiki"
  url "https://github.com/Rudde/mktorrent/archive/v1.1.tar.gz"
  sha256 "d0f47500192605d01b5a2569c605e51ed319f557d24cfcbcb23a26d51d6138c9"
  license "GPL-2.0"
  revision 1

  bottle do
    cellar :any
    sha256 "d4f6a644ffc64e3ffba7559569abc381de67f4cbc64245d6c1548ebb1cb5262d" => :big_sur
    sha256 "52e68c18ada643d382daa660c5bc697a6a6559abeba4138e87b722054668edf8" => :arm64_big_sur
    sha256 "3c9a180d450b8e49d1c4a6fc967df8599f602f955b7b27f8589b2052e0d77a91" => :catalina
    sha256 "22bc8649ce5fea25549610eec4110d45f3fa1d05335cfc982df82806ff34d71b" => :mojave
    sha256 "60be732dfea657c6faffa7e9d644f6ade7f974e7fea6ec46fa2941baac5eee80" => :high_sierra
    sha256 "3e7f91587dbea47713351b40a99b50728a878a9eb720eca14bd125541e62606f" => :sierra
  end

  depends_on "openssl@1.1"

  def install
    system "make", "USE_PTHREADS=1", "USE_OPENSSL=1", "USE_LONG_OPTIONS=1"
    bin.install "mktorrent"
  end

  test do
    (testpath/"test.txt").write <<~EOS
      Injustice anywhere is a threat to justice everywhere.
    EOS

    system bin/"mktorrent", "-d", "-c", "Martin Luther King Jr", "test.txt"
    assert_predicate testpath/"test.txt.torrent", :exist?, "Torrent was not created"

    file = File.read(testpath/"test.txt.torrent")
    output = file.force_encoding("ASCII-8BIT") if file.respond_to?(:force_encoding)
    assert_match "Martin Luther King Jr", output
  end
end
