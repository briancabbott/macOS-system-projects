class Syck < Formula
  desc "Extension for reading and writing YAML"
  homepage "https://github.com/indeyets/syck"
  url "https://github.s3.amazonaws.com/downloads/indeyets/syck/syck-0.70.tar.gz"
  sha256 "4c94c472ee8314e0d76eb2cca84f6029dc8fc58bfbc47748d50dcb289fda094e"
  # it is dual licensed, but the other license is not listed in SPDX
  license "MIT"

  livecheck do
    skip "Not maintained"
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "bd7841201573c4b79fc9f28a1b5a981fb138553e5a3fc8712ccba26b16059504" => :big_sur
    sha256 "00e98131174e989c13cde7eb0564b19581f0a60f0a5786c39d017ba7cd1ae893" => :arm64_big_sur
    sha256 "cc1bf3857c3d847854b54f45a25c5b1696c811f369c811521a07e2f5a4431e81" => :catalina
    sha256 "4baab04685bbb2fb536170a1bd069cfdec7fb5b2bc802ce6a80a4be933d8530a" => :mojave
    sha256 "a5a983847ae0e83a5f1d83adac736c602f7cce9ae00b9700d81ed766b4fe54bb" => :high_sierra
    sha256 "5a5f8c0f9a9def7ab59a052d85fbb128bc9a1c9c9b7ea2f9639109147eeba252" => :sierra
    sha256 "a2270f693ce6e8f0542f7d57dc6c8fcb731a268aee3c75a314523446c68602d6" => :el_capitan
    sha256 "cacc98eb6fab2440d3f69708688cb80b54bd14e5c472b7097ab9e235d5a670d6" => :yosemite
    sha256 "17606e97952cbea53c5649814d0d86c0cc2cdc5893b8063dcb4be8b4606c4f40" => :mavericks
  end

  def install
    ENV.deparallelize # Not parallel safe.
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
