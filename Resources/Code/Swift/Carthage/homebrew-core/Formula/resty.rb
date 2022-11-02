class Resty < Formula
  desc "Command-line REST client that can be used in pipelines"
  homepage "https://github.com/micha/resty"
  url "https://github.com/micha/resty/archive/v3.0.tar.gz"
  sha256 "9ed8f50dcf70a765b3438840024b557470d7faae2f0c1957a011ebb6c94b9dd1"
  license "MIT"
  head "https://github.com/micha/resty.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "03999dd31f795810e8febc7e5e206ff549d30f80062b6ea7a8235707b5b101f9" => :big_sur
    sha256 "25695263de11d4434bf21750710883185d3630a38d201b7050105296cf503f90" => :arm64_big_sur
    sha256 "cb5ad84cbacf18282a5ad172a48471d0e7ac007e4799f358fff049b8309aa27f" => :catalina
    sha256 "beee774062f1c32a72f203d0c8c5b0900ce85589c32b385ade712b74e5e1c73b" => :mojave
    sha256 "e65c38b826157c35f2e3acd50846be691b6b1a6231a23c62567c24a052d0dc7e" => :high_sierra
    sha256 "fb754eb95b4cb573eef1807f5dcddab59e021a4326022a9fb8126fb8e80ff247" => :sierra
    sha256 "435854dd9bc54f09e46f3f895fc0801ce90a30b23b8d9f109f361f89666fcfe1" => :el_capitan
  end

  uses_from_macos "perl"

  conflicts_with "nss", because: "both install `pp` binaries"

  resource "JSON" do
    url "https://cpan.metacpan.org/authors/id/I/IS/ISHIGAKI/JSON-2.94.tar.gz"
    sha256 "12271b5cee49943bbdde430eef58f1fe64ba6561980b22c69585e08fc977dc6d"
  end

  def install
    pkgshare.install "resty"

    ENV.prepend_create_path "PERL5LIB", libexec/"lib/perl5"

    resource("JSON").stage do
      system "perl", "Makefile.PL", "INSTALL_BASE=#{libexec}"
      system "make"
      system "make", "install"
    end

    bin.install "pp"
    bin.env_script_all_files(libexec/"bin", PERL5LIB: ENV["PERL5LIB"])

    bin.install "pypp"
  end

  def caveats
    <<~EOS
      To activate the resty, add the following at the end of your #{shell_profile}:
      source #{opt_pkgshare}/resty
    EOS
  end

  test do
    cmd = "zsh -c '. #{pkgshare}/resty && resty https://api.github.com' 2>&1"
    assert_equal "https://api.github.com*", shell_output(cmd).chomp
    json_pretty_pypp=<<~EOS
      {
          "a": 1
      }
    EOS
    json_pretty_pp=<<~EOS
      {
         "a" : 1
      }
    EOS
    assert_equal json_pretty_pypp, pipe_output("#{bin}/pypp", '{"a":1}', 0)
    assert_equal json_pretty_pp, pipe_output("#{bin}/pp", '{"a":1}', 0).chomp
  end
end
