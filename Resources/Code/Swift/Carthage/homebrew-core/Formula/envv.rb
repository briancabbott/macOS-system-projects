class Envv < Formula
  desc "Shell-independent handling of environment variables"
  homepage "https://github.com/jakewendt/envv#readme"
  url "https://github.com/jakewendt/envv/archive/v1.7.tar.gz"
  sha256 "1db05b46904e0cc4d777edf3ea14665f6157ade0567359e28663b5b00f6fa59a"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "39f8b46cce79836ebbc2281f1836a30eb2440e5af70bdc251469c0cca36f7828" => :big_sur
    sha256 "21e6f6e3c94dd0f14178ba1d5a53317bf1a6bf269762b5b79d9f93eff1ae3f00" => :arm64_big_sur
    sha256 "54b7b425a3db83134fc9038b8672bd84a943413f5386d9cef92711eeaaade467" => :catalina
    sha256 "59acc1f13ed58898376a14ffcb23766f62ff7c0446eebb3ee8aa1f8162f0994c" => :mojave
    sha256 "35e2781067a3f5429c36546a20faca9d4762882bf3908122efc58c8b752968e9" => :high_sierra
    sha256 "cc30a2317f78124c609d6313a33cea58c9d428a95903966da4cb42051630ef97" => :sierra
    sha256 "3b2fb0b35749280461b3982797ceea34bfa42d63cb5c6547986cf106669ee744" => :el_capitan
    sha256 "90a718606ec61e5a0e494d3e41b7d87048de803567f4ba2c65231fe41880bd97" => :yosemite
    sha256 "9ac7617d6475a67c60604fcd72d0ae1a5515df331944e8fb2c2a9223c16e3504" => :mavericks
  end

  def install
    system "make"

    bin.install "envv"
    man1.install "envv.1"
  end

  test do
    ENV["mylist"] = "A:B:C"
    assert_equal "mylist=A:C; export mylist", shell_output("#{bin}/envv del mylist B").strip
    assert_equal "mylist=B:C; export mylist", shell_output("#{bin}/envv del mylist A").strip
    assert_equal "mylist=A:B; export mylist", shell_output("#{bin}/envv del mylist C").strip

    assert_equal "", shell_output("#{bin}/envv add mylist B").strip
    assert_equal "mylist=B:A:C; export mylist", shell_output("#{bin}/envv add mylist B 1").strip
    assert_equal "mylist=A:C:B; export mylist", shell_output("#{bin}/envv add mylist B 99").strip

    assert_equal "mylist=A:B:C:D; export mylist", shell_output("#{bin}/envv add mylist D").strip
    assert_equal "mylist=D:A:B:C; export mylist", shell_output("#{bin}/envv add mylist D 1").strip
    assert_equal "mylist=A:B:D:C; export mylist", shell_output("#{bin}/envv add mylist D 3").strip
  end
end
