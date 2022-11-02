class Abcde < Formula
  desc "Better CD Encoder"
  homepage "https://abcde.einval.com"
  url "https://abcde.einval.com/download/abcde-2.9.3.tar.gz"
  sha256 "046cd0bba78dd4bbdcbcf82fe625865c60df35a005482de13a6699c5a3b83124"
  license "GPL-2.0-or-later"
  revision 1
  head "https://git.einval.com/git/abcde.git"

  bottle do
    cellar :any
    sha256 "c9668232e677e92b51210a0563c2156f030837b1fb221de60d16c83c466620b2" => :big_sur
    sha256 "4240ff000419b4ca9c0d275d70fccb10255ea17718906768892ba3a2d7ecb444" => :arm64_big_sur
    sha256 "fa00c7fc4b6b4ab794439f619ba00961358a4e5684a0ae2412fbd78ba2497df1" => :catalina
    sha256 "c2ef29d1b906767727d858047ddab9516f14c70073b30174a6222b1300446432" => :mojave
    sha256 "2a81af2921befb14f0a96e66ebc3884bd33f8bab156f7dc27e2816a956d033cb" => :high_sierra
  end

  depends_on "pkg-config" => :build

  depends_on "cdrtools"
  depends_on "eye-d3"
  depends_on "flac"
  depends_on "glyr"
  depends_on "lame"
  depends_on "libdiscid"
  depends_on "mkcue"
  depends_on "perl"
  depends_on "vorbis-tools"

  resource "MusicBrainz::DiscID" do
    url "https://cpan.metacpan.org/authors/id/N/NJ/NJH/MusicBrainz-DiscID-0.06.tar.gz"
    sha256 "ba0b6ed09897ff563ba59872ee93715bef37157515b19b7c6d6f286e6548ecab"
  end

  resource "WebService::MusicBrainz" do
    url "https://cpan.metacpan.org/authors/id/B/BF/BFAIST/WebService-MusicBrainz-1.0.5.tar.gz"
    sha256 "523b839968206c5751ea9ee670c7892c8c3be0f593aa591a00c0315468d09099"
  end

  resource "Mojo::Base" do
    url "https://cpan.metacpan.org/authors/id/S/SR/SRI/Mojolicious-8.64.tar.gz"
    sha256 "547a2c592e30ab5f22e42af9a84982b5cd699553f51226b6ed9524b4b7f4b24d"
  end

  def install
    ENV.prepend_create_path "PERL5LIB", libexec/"lib/perl5"

    resources.each do |r|
      r.stage do
        system "perl", "Makefile.PL", "INSTALL_BASE=#{libexec}"
        system "make", "install"
      end
    end

    inreplace "abcde-musicbrainz-tool", "#!/usr/bin/perl", "#!/usr/bin/env perl"

    system "make", "install", "prefix=#{prefix}", "sysconfdir=#{etc}"

    bin.env_script_all_files(libexec/"bin", PERL5LIB: ENV["PERL5LIB"])
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/abcde -v")
  end
end
