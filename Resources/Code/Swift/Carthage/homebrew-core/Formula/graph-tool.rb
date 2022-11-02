class GraphTool < Formula
  include Language::Python::Virtualenv

  desc "Efficient network analysis for Python 3"
  homepage "https://graph-tool.skewed.de/"
  url "https://downloads.skewed.de/graph-tool/graph-tool-2.37.tar.bz2"
  sha256 "df472a0debb683d2f58243945bb9fd525dfdac9ee0b1e9e70f054cff49235170"
  license "LGPL-3.0"

  livecheck do
    url "https://downloads.skewed.de/graph-tool/"
    regex(/href=.*?graph-tool[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "8edad8f578e7c6b9a8a0c3cf9b521471c1bf7c0a71709dc1150e414def4d8dab" => :big_sur
    sha256 "69579dc0987782c91a1d41af09bfe8fb2bcf62fc81090195d3cf532041ab35e1" => :arm64_big_sur
    sha256 "5f37c6b03cdb754495bc6d54cb1f5d691a9d4caf53a7bc51f2ddabc1984dc3cf" => :catalina
    sha256 "7da1c5cc5ef61975d4f61f4c6dd01c7e44b917e5b745b6031be4988fa334b71c" => :mojave
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "boost"
  depends_on "boost-python3"
  depends_on "cairomm@1.14"
  depends_on "cgal"
  depends_on "google-sparsehash"
  depends_on "gtk+3"
  depends_on "librsvg"
  depends_on macos: :mojave # for C++17
  depends_on "numpy"
  depends_on "py3cairo"
  depends_on "pygobject3"
  depends_on "python@3.9"
  depends_on "scipy"

  resource "Cycler" do
    url "https://files.pythonhosted.org/packages/c2/4b/137dea450d6e1e3d474e1d873cd1d4f7d3beed7e0dc973b06e8e10d32488/cycler-0.10.0.tar.gz"
    sha256 "cd7b2d1018258d7247a71425e9f26463dfb444d411c39569972f4ce586b0c9d8"
  end

  resource "kiwisolver" do
    url "https://files.pythonhosted.org/packages/16/e7/df58eb8868d183223692d2a62529a594f6414964a3ae93548467b146a24d/kiwisolver-1.1.0.tar.gz"
    sha256 "53eaed412477c836e1b9522c19858a8557d6e595077830146182225613b11a75"
  end

  resource "matplotlib" do
    url "https://files.pythonhosted.org/packages/12/d1/7b12cd79c791348cb0c78ce6e7d16bd72992f13c9f1e8e43d2725a6d8adf/matplotlib-3.1.1.tar.gz"
    sha256 "1febd22afe1489b13c6749ea059d392c03261b2950d1d45c17e3aed812080c93"
  end

  resource "pyparsing" do
    url "https://files.pythonhosted.org/packages/00/32/8076fa13e832bb4dcff379f18f228e5a53412be0631808b9ca2610c0f566/pyparsing-2.4.5.tar.gz"
    sha256 "4ca62001be367f01bd3e92ecbb79070272a9d4964dce6a48a82ff0b8bc7e683a"
  end

  resource "python-dateutil" do
    url "https://files.pythonhosted.org/packages/be/ed/5bbc91f03fa4c839c4c7360375da77f9659af5f7086b7a7bdda65771c8e0/python-dateutil-2.8.1.tar.gz"
    sha256 "73ebfe9dbf22e832286dafa60473e4cd239f8592f699aa5adaf10050e6e1823c"
  end

  resource "pytz" do
    url "https://files.pythonhosted.org/packages/82/c3/534ddba230bd4fbbd3b7a3d35f3341d014cca213f369a9940925e7e5f691/pytz-2019.3.tar.gz"
    sha256 "b02c06db6cf09c12dd25137e563b31700d3b80fcc4ad23abb7a315f2789819be"
  end

  resource "six" do
    url "https://files.pythonhosted.org/packages/94/3e/edcf6fef41d89187df7e38e868b2dd2182677922b600e880baad7749c865/six-1.13.0.tar.gz"
    sha256 "30f610279e8b2578cab6db20741130331735c781b56053c59c4076da27f06b66"
  end

  resource "zstandard" do
    url "https://files.pythonhosted.org/packages/1b/a7/97b157508923ec0c2d27cdc23003cb096fa50ae38ded6e54adcbca3dca35/zstandard-0.14.0.tar.gz"
    sha256 "9052398da52e8702cf9929999c8986b0f68b18c793e309cd8dff5cb7863d7652"
  end

  def install
    system "autoreconf", "-fiv"
    xy = Language::Python.major_minor_version Formula["python@3.9"].opt_bin/"python3"
    venv = virtualenv_create(libexec, Formula["python@3.9"].opt_bin/"python3")

    resources.each do |r|
      venv.pip_install_and_link r
    end

    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --prefix=#{prefix}
      PYTHON=python3
      PYTHON_LIBS=-undefined\ dynamic_lookup
      --with-python-module-path=#{lib}/python#{xy}/site-packages
      --with-boost-python=boost_python#{xy.to_s.delete(".")}-mt
      --with-boost-libdir=#{HOMEBREW_PREFIX}/opt/boost/lib
      --with-boost-coroutine=boost_coroutine-mt
    ]
    args << "--with-expat=#{MacOS.sdk_path}/usr" if MacOS.sdk_path_if_needed

    system "./configure", *args
    system "make", "install"

    site_packages = "lib/python#{xy}/site-packages"
    pth_contents = "import site; site.addsitedir('#{libexec/site_packages}')\n"
    (prefix/site_packages/"homebrew-graph-tool.pth").write pth_contents
  end

  test do
    (testpath/"test.py").write <<~EOS
      import graph_tool as gt
      g = gt.Graph()
      v1 = g.add_vertex()
      v2 = g.add_vertex()
      e = g.add_edge(v1, v2)
      assert g.num_edges() == 1
      assert g.num_vertices() == 2
    EOS
    system Formula["python@3.9"].opt_bin/"python3", "test.py"
  end
end
