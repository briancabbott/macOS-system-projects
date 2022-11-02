class Ccal < Formula
  desc "Create Chinese calendars for print or browsing"
  # no https urls
  homepage "http://ccal.chinesebay.com/ccal/ccal.htm"
  url "http://ccal.chinesebay.com/ccal/ccal-2.5.3.tar.gz"
  sha256 "3d4cbdc9f905ce02ab484041fbbf7f0b7a319ae6a350c6c16d636e1a5a50df96"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "82e5a0c59583063fdfa23e254f77ac5d7972a8fb5a3e36138233c7a47245abdf" => :big_sur
    sha256 "e9555bba354683597a63cf86554624a01afbf7cc897c0e292b10edc3657f4572" => :arm64_big_sur
    sha256 "ea42afd04ed210cf6e0bedac3ab4ce6b3e37421ba8d79478769d2e117c38a41f" => :catalina
    sha256 "c3a4bead8506e0234e878727e6d7827925e600bcee3857859fd575d4bbb185cc" => :mojave
    sha256 "cd9bd38878cee9658e312142edfca7cf35e5223ef30b3a3effc9e4108ccf3d51" => :high_sierra
  end

  def install
    system "make", "-e", "BINDIR=#{bin}", "install"
    system "make", "-e", "MANDIR=#{man}", "install-man"
  end

  test do
    assert_match "Year JiaWu, Month 1X", shell_output("#{bin}/ccal 2 2014")
  end
end
