class Nq < Formula
  desc "Unix command-line queue utility"
  homepage "https://github.com/chneukirchen/nq"
  url "https://github.com/chneukirchen/nq/archive/v0.3.1.tar.gz"
  sha256 "8897a747843fe246a6f8a43e181ae79ef286122a596214480781a02ef4ea304b"
  license "CC0-1.0"
  head "https://github.com/chneukirchen/nq.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "97df5cbf63b142bf49bbbe683f869a96b003ab3c30eee3ae36ad0ee741744b1c" => :big_sur
    sha256 "e20a81316a20ce81b4396831d87a88e5e7025a5d7792116352297882565875ec" => :arm64_big_sur
    sha256 "95011ee6d48728704ee95480374c545d3c2bcea8f4482cecd9b8dbbab9a2407b" => :catalina
    sha256 "b5b3f7b76cc79a5bc6d4a55e4fb3e018b08052dc7faa173300b1ddf2e16e6bee" => :mojave
    sha256 "a6d18f2d7f1fafd661a5d145599969707efe71969ccc6ac34593f3f60c59081a" => :high_sierra
    sha256 "0e8d6557f7713be4c1e5074ea909d36cd12e2e17d85a1c0a1141ac64f06953d3" => :sierra
    sha256 "67374f5db8a35f877a16e0fdbd313276fb269db81ce49e7654fb61fa865417cd" => :el_capitan
  end

  def install
    system "make", "all", "PREFIX=#{prefix}"
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    system "#{bin}/nq", "touch", "TEST"
    assert_match /exited with status 0/, shell_output("#{bin}/fq -a")
    assert_predicate testpath/"TEST", :exist?
  end
end
