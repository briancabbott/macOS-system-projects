class Xapian < Formula
  desc "C++ search engine library"
  homepage "https://xapian.org/"
  url "https://oligarchy.co.uk/xapian/1.4.17/xapian-core-1.4.17.tar.xz"
  sha256 "b5eb8556dea1b0cad4167a66223522e66d670ec1eba16c7fdc844ed6b652572e"
  license "GPL-2.0"
  revision 1
  version_scheme 1

  livecheck do
    url :homepage
    regex(/latest stable version.*?is v?(\d+(?:\.\d+)+)</im)
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "5645bdbfbf7ebb1e0d068bb51cce7a28c1e4102a29da6647b5959d0607100d21" => :big_sur
    sha256 "255bee7728a81eaf941120a7085c724b57dfe074de8a8987cc0403d01572fef1" => :arm64_big_sur
    sha256 "f9e6103d9938f2708d0d5d38030c833c5f78d02efe53091616d1fd9e645a69ee" => :catalina
    sha256 "9ad6e312f0949659d3d96f7ef1edc63cd4e8b2a45e7a94243ffa4cb3abf940f8" => :mojave
  end

  depends_on "sphinx-doc" => :build
  depends_on "python@3.9"

  uses_from_macos "zlib"

  on_linux do
    depends_on "util-linux"
  end

  skip_clean :la

  resource "bindings" do
    url "https://oligarchy.co.uk/xapian/1.4.17/xapian-bindings-1.4.17.tar.xz"
    sha256 "48a65d91e0c3a4a8f4a1ca05dc39225912088aca2c47c0048cc93b09d338ebd3"
  end

  def install
    python = Formula["python@3.9"].opt_bin/"python3"
    ENV["PYTHON"] = python

    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"

    resource("bindings").stage do
      ENV["XAPIAN_CONFIG"] = bin/"xapian-config"

      xy = Language::Python.major_minor_version python
      ENV.prepend_create_path "PYTHON3_LIB", lib/"python#{xy}/site-packages"

      ENV.append_path "PYTHONPATH", Formula["sphinx-doc"].opt_libexec/"lib/python#{xy}/site-packages"
      ENV.append_path "PYTHONPATH", Formula["sphinx-doc"].opt_libexec/"vendor/lib/python#{xy}/site-packages"

      # Fix build on Big Sur (darwin20)
      # https://github.com/xapian/xapian/pull/319
      inreplace "configure", "*-darwin[91]*", "*-darwin[912]*"

      system "./configure", "--disable-dependency-tracking",
                            "--prefix=#{prefix}",
                            "--with-python3"

      system "make", "install"
    end
  end

  test do
    system bin/"xapian-config", "--libs"
    system Formula["python@3.9"].opt_bin/"python3", "-c", "import xapian"
  end
end
