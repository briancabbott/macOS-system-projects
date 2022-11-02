class Mytop < Formula
  desc "Top-like query monitor for MySQL"
  homepage "https://web.archive.org/web/20200221154243/www.mysqlfanboy.com/mytop-3/"
  url "https://web.archive.org/web/20150602163826/www.mysqlfanboy.com/mytop-3/mytop-1.9.1.tar.gz"
  mirror "https://deb.debian.org/debian/pool/main/m/mytop/mytop_1.9.1.orig.tar.gz"
  sha256 "179d79459d0013ab9cea2040a41c49a79822162d6e64a7a85f84cdc44828145e"
  revision 8

  livecheck do
    skip "Upstream is gone and the formula uses archive.org URLs"
  end

  bottle do
    cellar :any
    sha256 "ea2f5229c929cb23466f75964d1bf294130381b27efd55cf2ce91cb248c43732" => :big_sur
    sha256 "1b5b8d90532a1d9712a2f0212374975af97603ccdcf7e452bc13178aedfce966" => :arm64_big_sur
    sha256 "69930f7d5c68b0d6ce75c89820732f269d3b3c6651358875b0db58ae1ead38f0" => :catalina
    sha256 "ac13ecf239ff9d4bb1d39ad584c46ac9a5c95f3b96b3991bf9108280b30c0a19" => :mojave
    sha256 "2862de7630947648898e1ef348a8357fdd25622310c9af03450c40ea33fc925c" => :high_sierra
  end

  depends_on "mysql-client"
  depends_on "openssl@1.1"

  uses_from_macos "perl"

  conflicts_with "mariadb", because: "both install `mytop` binaries"

  resource "List::Util" do
    url "https://cpan.metacpan.org/authors/id/P/PE/PEVANS/Scalar-List-Utils-1.46.tar.gz"
    sha256 "30662b1261364adb317e9a5bd686273d3dd731e3fda1b8e894802aa52e0052e7"
  end

  resource "Config::IniFiles" do
    url "https://cpan.metacpan.org/authors/id/S/SH/SHLOMIF/Config-IniFiles-2.94.tar.gz"
    sha256 "d6d38a416da79de874c5f1825221f22e972ad500b6527d190cc6e9ebc45194b4"
  end

  # In Mojave, this is not part of the system Perl anymore
  if MacOS.version >= :mojave
    resource "DBI" do
      url "https://cpan.metacpan.org/authors/id/T/TI/TIMB/DBI-1.641.tar.gz"
      sha256 "5509e532cdd0e3d91eda550578deaac29e2f008a12b64576e8c261bb92e8c2c1"
    end
  end

  resource "DBD::mysql" do
    url "https://cpan.metacpan.org/authors/id/C/CA/CAPTTOFU/DBD-mysql-4.046.tar.gz"
    sha256 "6165652ec959d05b97f5413fa3dff014b78a44cf6de21ae87283b28378daf1f7"
  end

  # Pick up some patches from Debian to improve functionality & fix
  # some syntax warnings when using recent versions of Perl.
  patch do
    url "https://deb.debian.org/debian/pool/main/m/mytop/mytop_1.9.1-2.debian.tar.xz"
    sha256 "9c97b7d2a2d4d169c5f263ce0adb6340b71e3a0afd4cdde94edcead02421489a"
    apply "patches/01_fix_pod.patch",
          "patches/02_remove_db_test.patch",
          "patches/03_fix_newlines.patch",
          "patches/04_fix_unitialized.patch",
          "patches/05_prevent_ctrl_char_printing.patch",
          "patches/06_fix_screenwidth.patch",
          "patches/07_add_doc_on_missing_cli_options.patch",
          "patches/08_add_mycnf.patch",
          "patches/09_q_is_quit.patch",
          "patches/10_fix_perl_warnings.patch",
          "patches/13_fix_scope_for_show_slave_status_data.patch"
  end

  def install
    ENV.prepend_create_path "PERL5LIB", libexec/"lib/perl5"
    resources.each do |r|
      r.stage do
        system "perl", "Makefile.PL", "INSTALL_BASE=#{libexec}"
        system "make", "install"
      end
    end

    system "perl", "Makefile.PL", "INSTALL_BASE=#{prefix}"
    system "make", "test", "install"
    share.install prefix/"man"
    bin.env_script_all_files(libexec/"bin", PERL5LIB: ENV["PERL5LIB"])
  end

  test do
    assert_match "username you specified", pipe_output("#{bin}/mytop 2>&1")
  end
end
