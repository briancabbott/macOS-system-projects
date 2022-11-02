class TtyClock < Formula
  desc "Digital clock in ncurses"
  homepage "https://github.com/xorg62/tty-clock"
  url "https://github.com/xorg62/tty-clock/archive/v2.3.tar.gz"
  sha256 "343e119858db7d5622a545e15a3bbfde65c107440700b62f9df0926db8f57984"
  license "BSD-3-Clause"
  head "https://github.com/xorg62/tty-clock.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "fd72f43c25837763c243876436de51d99369fb8f540171aec16b2f66cb2870e3" => :big_sur
    sha256 "fcae9d0e0eeaf68815b4a521f7f75c352d4188a38652b4841bd48b608120edce" => :arm64_big_sur
    sha256 "dc5a60415f5cd5397d973b361db6bc0db2172621fe6eed037ee05c851097c27d" => :catalina
    sha256 "eab206747869e0190d82dfa71d7763df4a3f202c3035f7bccb5b32fc52580989" => :mojave
    sha256 "b3d2a19cdb38e0e156be552d6f9ca8926097300f17bbe6628b7443934d3e1cb1" => :high_sierra
    sha256 "9b0e056ec6d86d9ba9cbd2abc02236607a6ad5601e7a656d10cad20182564315" => :sierra
    sha256 "c0d981769811bf1c265e11702ea0d26bcf87102ac92896c04c14a91fbed1cc8c" => :el_capitan
    sha256 "9341fb07070b665dc5f9593c1b4811ec734f7221afbde2547cee55fc9102aa1e" => :yosemite
  end

  depends_on "pkg-config" => :build

  def install
    ENV.append "LDFLAGS", "-lncurses"
    system "make", "PREFIX=#{prefix}"
    system "make", "PREFIX=#{prefix}", "install"
  end

  test do
    system "#{bin}/tty-clock", "-i"
  end
end
