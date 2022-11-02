class Imapsync < Formula
  desc "Migrate or backup IMAP mail accounts"
  homepage "https://imapsync.lamiral.info/"
  url "https://imapsync.lamiral.info/dist2/imapsync-1.977.tgz"
  # NOTE: The mirror will return 404 until the version becomes outdated.
  sha256 "1ce601150568a6b13a5d8730bee07fdc05b35f3f4e35775f1b471ba221940c2a"
  license "NLPL"
  head "https://github.com/imapsync/imapsync.git"

  livecheck do
    url "https://imapsync.lamiral.info/dist2/"
    regex(/href=.*?imapsync[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "88c192e6f93482a9ed5902b1b369cd9f788611f256889ed9650888999edce9fd" => :big_sur
    sha256 "3c53f64995cf7520fbd270813df2d9437b243980181a7dff27dd24345707d66b" => :arm64_big_sur
    sha256 "6c1a1bed4c3ad926f34a5efe6b6bfe5d2776ed4ee0d000679b21679c47ff81cb" => :catalina
    sha256 "5b259d1a7096a887af61eef5a379122c8e94e71f912b2670a35031e6c159351c" => :mojave
    sha256 "fa0a19bf66b24d332f6e0f4f827b2abdff2b904b483c4927f2ed7d6d944ab261" => :high_sierra
  end

  uses_from_macos "perl"

  resource "Encode::IMAPUTF7" do
    url "https://cpan.metacpan.org/authors/id/P/PM/PMAKHOLM/Encode-IMAPUTF7-1.05.tar.gz"
    sha256 "470305ddc37483cfe8d3c16d13770a28011f600bf557acb8c3e07739997c37e1"
  end

  resource "Unicode::String" do
    url "https://cpan.metacpan.org/authors/id/G/GA/GAAS/GAAS/Unicode-String-2.10.tar.gz"
    sha256 "894a110ece479546af8afec0972eec7320c86c4dea4e6b354dff3c7526ba9b68"
  end

  resource "File::Copy::Recursive" do
    url "https://cpan.metacpan.org/authors/id/D/DM/DMUEY/File-Copy-Recursive-0.44.tar.gz"
    sha256 "ae19a0b58dc1b3cded9ba9cfb109288d8973d474c0b4bfd28b27cf60e8ca6ee4"
  end

  resource "Authen::NTLM" do
    url "https://cpan.metacpan.org/authors/id/N/NB/NBEBOUT/NTLM-1.09.tar.gz"
    sha256 "c823e30cda76bc15636e584302c960e2b5eeef9517c2448f7454498893151f85"
  end

  resource "Mail::IMAPClient" do
    url "https://cpan.metacpan.org/authors/id/P/PL/PLOBBES/Mail-IMAPClient-3.42.tar.gz"
    sha256 "1c2264d50c54c839a3e38ce2f8edda3d24f30cc607940d7574beab19cb00ce7e"
  end

  resource "IO::Tee" do
    url "https://cpan.metacpan.org/authors/id/N/NE/NEILB/IO-Tee-0.65.tar.gz"
    sha256 "c63dcd109b268962f867407da2654282e3c85113dc7e9655fe8a62331d490c12"
  end

  resource "Data::Uniqid" do
    url "https://cpan.metacpan.org/authors/id/M/MW/MWX/Data-Uniqid-0.12.tar.gz"
    sha256 "b6919ba49b9fe98bfdf3e8accae7b9b7f78dc9e71ebbd0b7fef7a45d99324ccb"
  end

  resource "JSON" do
    url "https://cpan.metacpan.org/authors/id/I/IS/ISHIGAKI/JSON-4.02.tar.gz"
    sha256 "444a88755a89ffa2a5424ab4ed1d11dca61808ebef57e81243424619a9e8627c"
  end

  resource "Test::MockObject" do
    url "https://cpan.metacpan.org/authors/id/C/CH/CHROMATIC/Test-MockObject-1.20180705.tar.gz"
    sha256 "4516058d5d511155c1c462dab4027d762d6a474b99f73bf7da20b5ffbd024518"
  end

  resource "JSON::WebToken" do
    url "https://cpan.metacpan.org/authors/id/X/XA/XAICRON/JSON-WebToken-0.10.tar.gz"
    sha256 "77c182a98528f1714d82afc548d5b3b4dc93e67069128bb9b9413f24cf07248b"
  end

  resource "Module::Build::Tiny" do
    url "https://cpan.metacpan.org/authors/id/L/LE/LEONT/Module-Build-Tiny-0.039.tar.gz"
    sha256 "7d580ff6ace0cbe555bf36b86dc8ea232581530cbeaaea09bccb57b55797f11c"
  end

  resource "Readonly" do
    url "https://cpan.metacpan.org/authors/id/S/SA/SANKO/Readonly-2.05.tar.gz"
    sha256 "4b23542491af010d44a5c7c861244738acc74ababae6b8838d354dfb19462b5e"
  end

  resource "Sys::MemInfo" do
    url "https://cpan.metacpan.org/authors/id/S/SC/SCRESTO/Sys-MemInfo-0.99.tar.gz"
    sha256 "0786319d3a3a8bae5d727939244bf17e140b714f52734d5e9f627203e4cf3e3b"
  end

  resource "File::Tail" do
    url "https://cpan.metacpan.org/authors/id/M/MG/MGRABNAR/File-Tail-1.3.tar.gz"
    sha256 "26d09f81836e43eae40028d5283fe5620fe6fe6278bf3eb8eb600c48ec34afc7"
  end

  resource "IO::Socket::IP" do
    url "https://cpan.metacpan.org/authors/id/P/PE/PEVANS/IO-Socket-IP-0.39.tar.gz"
    sha256 "11950da7636cb786efd3bfb5891da4c820975276bce43175214391e5c32b7b96"
  end

  def install
    ENV.prepend_create_path "PERL5LIB", libexec/"lib/perl5"

    build_pl = ["JSON::WebToken", "Module::Build::Tiny", "Readonly"]

    resources.each do |r|
      r.stage do
        next if build_pl.include? r.name

        system "perl", "Makefile.PL", "INSTALL_BASE=#{libexec}"
        system "make"
        system "make", "install"
      end
    end

    build_pl.each do |name|
      resource(name).stage do
        system "perl", "Build.PL", "--install_base", libexec
        system "./Build"
        system "./Build", "install"
      end
    end

    system "perl", "-c", "imapsync"
    system "pod2man", "imapsync", "imapsync.1"
    inreplace "imapsync", "#!/usr/bin/env perl", "#!/usr/bin/perl"
    bin.install "imapsync"
    man1.install "imapsync.1"
    bin.env_script_all_files(libexec/"bin", PERL5LIB: ENV["PERL5LIB"])
  end

  test do
    assert_match version.to_s, pipe_output("#{bin}/imapsync --dry")
    shell_output("#{bin}/imapsync --dry \
       --host1 test1.lamiral.info --user1 test1 --password1 secret1 \
       --host2 test2.lamiral.info --user2 test2 --password2 secret2")
  end
end
