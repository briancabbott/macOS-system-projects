class RmImproved < Formula
  desc "Command-line deletion tool focused on safety, ergonomics, and performance"
  homepage "https://github.com/nivekuil/rip"
  url "https://github.com/nivekuil/rip/archive/0.13.1.tar.gz"
  sha256 "73acdc72386242dced117afae43429b6870aa176e8cc81e11350e0aaa95e6421"
  license "GPL-3.0-or-later"
  head "https://github.com/nivekuil/rip.git"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "16260eaa3888976a39b9711ea7150d9e7e3afbee0c34efa022b1a2542f5c4bd9" => :big_sur
    sha256 "6b404b0fe096447d90c21c15140ee9295fdea4060771723e818625e8dcde8e2f" => :catalina
    sha256 "cd164204efca72560dcb8d39db760d7e9efbeab5e9bfd0718c6cccd5b022a7f3" => :mojave
    sha256 "27fa7c0976c9361fae1638f05a0c756603a509a16459db688d2e787ceb123de2" => :high_sierra
  end

  depends_on "rust" => :build

  def install
    system "cargo", "install", *std_cargo_args
  end

  test do
    trash = testpath/"trash"
    ENV["GRAVEYARD"] = trash

    source_file = testpath/"testfile"
    deleted_file = Pathname.new File.join(trash, source_file)
    touch source_file

    system "rip", source_file
    assert_match deleted_file.to_s, shell_output("#{bin}/rip -s")
    assert_predicate deleted_file, :exist?

    system "rip", "-u", deleted_file
    assert_predicate source_file, :exist?
  end
end
