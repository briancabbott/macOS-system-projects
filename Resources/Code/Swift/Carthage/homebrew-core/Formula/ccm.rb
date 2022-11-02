class Ccm < Formula
  desc "Create and destroy an Apache Cassandra cluster on localhost"
  homepage "https://github.com/pcmanus/ccm"
  url "https://files.pythonhosted.org/packages/f1/12/091e82033d53b3802e1ead6b16045c5ecfb03374f8586a4ae4673a914c1a/ccm-3.1.5.tar.gz"
  sha256 "f07cc0a37116d2ce1b96c0d467f792668aa25835c73beb61639fa50a1954326c"
  license "Apache-2.0"
  revision 1
  head "https://github.com/pcmanus/ccm.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "f0884f1aa2af574e62b130656d2110c9ad8d544caf6c984e08943c05a0bbf630" => :big_sur
    sha256 "955fd74e54ceaed0772238587c7fe45600813c685cdd4c1af19a5c0c99462a1f" => :arm64_big_sur
    sha256 "22fd6d245793ee81816df4ecce043c3a7bda47809f7afacb0eb7d12c6cf23fa5" => :catalina
    sha256 "59242833ff1017ca8c5fa4a18d0ad7c1c6179b323daa473716c990db2c3d2e2f" => :mojave
    sha256 "d986995a2a6de7bce2eb538861c94c826bd7f0f38bd23fc999feef15204de8fa" => :high_sierra
  end

  depends_on "python@3.9"

  resource "PyYAML" do
    url "https://files.pythonhosted.org/packages/3d/d9/ea9816aea31beeadccd03f1f8b625ecf8f645bd66744484d162d84803ce5/PyYAML-5.3.tar.gz"
    sha256 "e9f45bd5b92c7974e59bcd2dcc8631a6b6cc380a904725fce7bc08872e691615"
  end

  resource "six" do
    url "https://files.pythonhosted.org/packages/21/9f/b251f7f8a76dec1d6651be194dfba8fb8d7781d10ab3987190de8391d08e/six-1.14.0.tar.gz"
    sha256 "236bdbdce46e6e6a3d61a337c0f8b763ca1e8717c03b369e87a7ec7ce1319c0a"
  end

  resource "cassandra-driver" do
    url "https://files.pythonhosted.org/packages/19/bd/b522b200e8a7cc5ace859e9667308a3a302a23d6df09ae087ca2dfbf60c2/cassandra-driver-3.22.0.tar.gz"
    sha256 "df825ee4ebb7f7fa33ab028d673530184fe0ee41ea66b2f9ddd478db56145a31"
  end

  def install
    xy = Language::Python.major_minor_version "python3"
    ENV.prepend_create_path "PYTHONPATH", libexec/"vendor/lib/python#{xy}/site-packages"
    %w[PyYAML six cassandra-driver].each do |r|
      resource(r).stage do
        system "python3", *Language::Python.setup_install_args(libexec/"vendor")
      end
    end

    ENV.prepend_create_path "PYTHONPATH", libexec/"lib/python#{xy}/site-packages"
    system "python3", *Language::Python.setup_install_args(libexec)

    bin.install Dir[libexec/"bin/*"]
    bin.env_script_all_files(libexec/"bin", PYTHONPATH: ENV["PYTHONPATH"])
  end

  test do
    assert_match "Usage", shell_output("#{bin}/ccm", 1)
  end
end
