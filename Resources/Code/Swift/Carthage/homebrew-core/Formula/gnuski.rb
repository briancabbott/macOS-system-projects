class Gnuski < Formula
  desc "Open source clone of Skifree"
  homepage "https://gnuski.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/gnuski/gnuski/gnuski-0.3/gnuski-0.3.tar.gz"
  sha256 "1b629bd29dd6ad362b56055ccdb4c7ad462ff39d7a0deb915753c2096f5f959d"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "6ae8c77ab5fed3e5f85be1c411cc631099d219bd49ddb3a35f527da27894b880" => :big_sur
    sha256 "d82abb34deac302a464e7c664a3dcf0726f1d0f8ee03586b195e3449a2c83d43" => :arm64_big_sur
    sha256 "7c58085b5ceb98168c728003d484e08410c837bde9b044ed5a5fa6f26796d9ab" => :catalina
    sha256 "4fe7b21e4b40ee72c7825c1e0330a958694b98529121385b78b7af9aff229d6d" => :mojave
    sha256 "6f15bd497951ea784e84b2ec888be83343ad1ad96eb6bab9ba343bff31246700" => :high_sierra
    sha256 "3874907a4ad715492c026d969ec3265dcd5f71424dde07a83aa1c21a1e36fa38" => :sierra
    sha256 "ce14d8ee8b8d58c710b93adb2f4cedfb9d78fb64746f38daee4ea38aa977ae43" => :el_capitan
    sha256 "3163ed8b9f1487e0f5f5a42006e0edfbfdb3a4dbea9b917c0aa692db282ec13b" => :yosemite
  end

  def install
    system "make"
    bin.install "gnuski"
  end
end
