class Gistit < Formula
  desc "Command-line utility for creating Gists"
  homepage "https://gistit.herokuapp.com/"
  url "https://github.com/jrbasso/gistit/archive/v0.1.3.tar.gz"
  sha256 "b7203c318460c264bd19b35a767da1cef33e5cae2c65b057e401fe20f47e1cca"
  license "MIT"
  head "https://github.com/jrbasso/gistit.git"

  bottle do
    cellar :any
    sha256 "041276422d7633ae3b0ae151939e41619387935a7b5afdcae919aeb6dd4927f1" => :big_sur
    sha256 "b84985ef5d5eb68a1260a2dea107c6ff4ec29ce42bb1f33321f5074cc088c457" => :arm64_big_sur
    sha256 "8a5288aec9e18273db915b7c2ac077157549f1b496a0fd98e878d34ec9be66fa" => :catalina
    sha256 "56cf73e5a5c9742640a146b1b37ed2aea557b356b2e5a96ff437130a6aab4dfb" => :mojave
    sha256 "decb56c455eb39e379b94b6832281ea06ae8a42c745eb6c108c15883a0ef2fad" => :high_sierra
    sha256 "269b7429070e11980d6764f2f6bd1d870d2e391cfd919f948159cb35cfab1184" => :sierra
    sha256 "b968a2885e3ac3e0c717fdabb9986149a39d58d031476d4ef3ada7a9e1ad07e9" => :el_capitan
    sha256 "052536cb990d1c5ea4b48612026dfb13addd03cbc06ed8c6b42f3636eb6771a7" => :yosemite
    sha256 "c2388d390d86f39934ae51e71edfa1fd890d8ad4384427ab14275280eece97ce" => :mavericks
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "jansson"

  def install
    mv "configure.in", "configure.ac" # silence warning
    system "./autogen.sh", "--disable-dependency-tracking",
                           "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end

  test do
    system "#{bin}/gistit", "-v"
  end
end
