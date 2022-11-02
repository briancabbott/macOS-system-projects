class Cdecl < Formula
  desc "Turn English phrases to C or C++ declarations"
  homepage "https://cdecl.org/"
  url "https://cdecl.org/files/cdecl-blocks-2.5.tar.gz"
  sha256 "9ee6402be7e4f5bb5e6ee60c6b9ea3862935bf070e6cecd0ab0842305406f3ac"

  bottle do
    cellar :any_skip_relocation
    sha256 "ac04af015afd9bc8d756f4220c53d484de0fba6f0c8a0976f99b63e2bdfccf3e" => :big_sur
    sha256 "6ffb24daa6ca1e12ddfbc0cf77ef3461dfdc7eec3ba8149e0f5dbacabeefce2b" => :arm64_big_sur
    sha256 "bba9953b96f037148b23ecf85030ed505bf1e6712f21099d494084c26cd52f1c" => :catalina
    sha256 "beed8e3f4c2de0b75bd12bd65e6d9ce4a7cb626fac5cd8c5e20426d2b9325840" => :mojave
    sha256 "a2469d514723e35850b252b97d3bf90f9311c276455b218383d276ccb0c88ee4" => :high_sierra
    sha256 "1d424613881cf9109d824664fc77fc947f2968b9850d448db4b02c6f0a562b5c" => :sierra
    sha256 "4f0e990d88823aa9f3d1dcea71ffa442c13640ce82cc9da41f90a1be5ef457dc" => :el_capitan
    sha256 "e8f53a0e5b3649f0c691c60380b9c77af573387240f3479a41550583fcc4e22c" => :yosemite
    sha256 "b1e1618d0f1bcbb801c669c314c36c72e47e8829950a8bf0899d0517f3036ccc" => :mavericks
  end

  def install
    # Fix namespace clash with Lion's getline
    inreplace "cdecl.c", "getline", "cdecl_getline"

    bin.mkpath
    man1.mkpath

    ENV.append "CFLAGS", "-DBSD -DUSE_READLINE -std=gnu89"

    system "make", "CC=#{ENV.cc}",
                   "CFLAGS=#{ENV.cflags}",
                   "LIBS=-lreadline",
                   "BINDIR=#{bin}",
                   "MANDIR=#{man1}",
                   "install"
  end

  test do
    assert_equal "declare a as pointer to int",
                 shell_output("#{bin}/cdecl explain int *a").strip
  end
end
