class Libdiscid < Formula
  desc "C library for creating MusicBrainz and freedb disc IDs"
  homepage "https://musicbrainz.org/doc/libdiscid"
  url "http://ftp.musicbrainz.org/pub/musicbrainz/libdiscid/libdiscid-0.6.2.tar.gz"
  mirror "https://ftp.osuosl.org/pub/musicbrainz/libdiscid/libdiscid-0.6.2.tar.gz"
  sha256 "f9e443ac4c0dd4819c2841fcc82169a46fb9a626352cdb9c7f65dd3624cd31b9"
  license "LGPL-2.1"

  livecheck do
    url "https://ftp.osuosl.org/pub/musicbrainz/libdiscid/"
    regex(/href=.*?libdiscid[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "3388368253a64c71bd0cb6fcf0cd06102808d53cbaf3be99e482f175b5129952" => :big_sur
    sha256 "67f29aadedf99093c49470c6b99974ea94ed16491bf173e141055c501f4f26e3" => :arm64_big_sur
    sha256 "74dd7ef5362b91818107ef3c8c3edab443faf8a17662294a24573e5f476110c7" => :catalina
    sha256 "f6a415ae56c151ccef5e10cc239675be8cbd7dcf60a8b9c88c87a756bda5bd9a" => :mojave
    sha256 "3ffb586f09efcd9322a28bafc671292d0caf38edc18326c048a7390ced94979f" => :high_sierra
    sha256 "6d43fee98239a6a600e59cce0f4f2ceda713bf27cc3d03bc8711d1c773ba84b6" => :sierra
    sha256 "22e96d837cfe404cf268c41f6ce26c6b47eb8a991578ce1f18bcea862f9f1c91" => :el_capitan
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
