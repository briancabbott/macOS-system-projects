class Osquery < Formula
  desc "SQL powered operating system instrumentation and analytics"
  homepage "https://osquery.io"
  url "https://github.com/facebook/osquery/archive/3.3.2.tar.gz"
  sha256 "74280181f45046209053a3e15114d93adc80929a91570cc4497931cfb87679e4"
  license any_of: ["Apache-2.0", "GPL-2.0-only"]
  revision 17

  bottle do
    cellar :any
    sha256 "ea852c037cabbc09798dcdba8ce675e49d56f3a576b3b1fbf541bd9ca40ba1d1" => :catalina
    sha256 "ec9daadf541bd30127bef694fc8e1ad1689de9338936ddbf28a0a138d25890b0" => :mojave
    sha256 "9c70ccbcc111293ceddc2421035cf6b0748709d96979c3db01a5d8dee1053db4" => :high_sierra
  end

  disable! date: "2020-05-18", because: "has old, vendored dependencies and cannot use duplicated Homebrew libraries"

  depends_on "bison" => :build
  depends_on "cmake" => :build
  depends_on "python@3.9" => :build
  depends_on "augeas"
  depends_on "boost"
  depends_on "gflags"
  depends_on "glog"
  depends_on "libarchive"
  depends_on "libmagic"
  depends_on "librdkafka"
  depends_on "lldpd"
  # osquery only supports macOS 10.12 and above. Do not remove this.
  depends_on macos: :sierra
  depends_on "openssl@1.1"
  depends_on "rapidjson"
  depends_on "rocksdb"
  depends_on "sleuthkit"
  depends_on "ssdeep"
  depends_on "thrift"
  depends_on "xz"
  depends_on "yara"
  depends_on "zstd"

  resource "MarkupSafe" do
    url "https://files.pythonhosted.org/packages/b9/2e/64db92e53b86efccfaea71321f597fa2e1b2bd3853d8ce658568f7a13094/MarkupSafe-1.1.1.tar.gz"
    sha256 "29872e92839765e546828bb7754a68c418d927cd064fd4708fab9fe9c8bb116b"
  end

  resource "Jinja2" do
    url "https://files.pythonhosted.org/packages/64/a7/45e11eebf2f15bf987c3bc11d37dcc838d9dc81250e67e4c5968f6008b6c/Jinja2-2.11.2.tar.gz"
    sha256 "89aab215427ef59c34ad58735269eb58b1a5808103067f7bb9d5836c651b3bb0"
  end

  resource "third-party" do
    url "https://github.com/osquery/third-party/archive/3.0.0.tar.gz"
    sha256 "98731b92147f6c43f679a4a9f63cbb22f2a4d400d94a45e308702dee66a8de9d"
  end

  resource "aws-sdk-cpp" do
    url "https://github.com/aws/aws-sdk-cpp/archive/1.4.55.tar.gz"
    sha256 "0a70c2998d29cc4d8a4db08aac58eb196d404073f6586a136d074730317fe408"
  end

  # Upstream fix for boost 1.69, remove in next version
  # https://github.com/facebook/osquery/pull/5496
  patch do
    url "https://github.com/facebook/osquery/commit/130b3b3324e2.patch?full_index=1"
    sha256 "b5bcb8a774423131be72dc7981227552f565a7102edb27b0644c1c904ff9949a"
  end

  # Patch for compatibility with OpenSSL 1.1
  # submitted upstream: https://github.com/osquery/osquery/issues/5755
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/85fa66a9/osquery/openssl-1.1.diff"
    sha256 "18ace03c11e06b0728060382284a8da115bd6e14247db20ac0188246e5ff8af4"
  end

  def install
    ENV.cxx11

    vendor = buildpath/"brew_vendor"

    resource("aws-sdk-cpp").stage do
      args = std_cmake_args + %W[
        -DSTATIC_LINKING=1
        -DNO_HTTP_CLIENT=1
        -DMINIMIZE_SIZE=ON
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_ONLY=ec2;firehose;kinesis;sts
        -DCMAKE_INSTALL_PREFIX=#{vendor}/aws-sdk-cpp
      ]

      mkdir "build" do
        system "cmake", "..", *args
        system "make"
        system "make", "install"
      end
    end

    # Skip test and benchmarking.
    ENV["SKIP_TESTS"] = "1"
    ENV["SKIP_DEPS"] = "1"

    # Skip SMART drive tables.
    # SMART requires a dependency that isn't packaged by brew.
    ENV["SKIP_SMART"] = "1"

    # Link dynamically against brew-installed libraries.
    ENV["BUILD_LINK_SHARED"] = "1"
    # Set the version
    ENV["OSQUERY_BUILD_VERSION"] = version

    xy = Language::Python.major_minor_version Formula["python@3.9"].opt_bin/"python3"
    ENV.prepend_create_path "PYTHONPATH", buildpath/"third-party/python/lib/python#{xy}/site-packages"

    res = resources.map(&:name).to_set - %w[aws-sdk-cpp third-party]
    res.each do |r|
      resource(r).stage do
        system Formula["python@3.9"].opt_bin/"python3",
               "setup.py", "install",
               "--prefix=#{buildpath}/third-party/python/",
               "--single-version-externally-managed",
               "--record=installed.txt"
      end
    end

    cxx_flags_release = %W[
      -DNDEBUG
      -I#{MacOS.sdk_path}/usr/include/libxml2
      -I#{vendor}/aws-sdk-cpp/include
    ]

    args = std_cmake_args + %W[
      -Daws-cpp-sdk-core_library:FILEPATH=#{vendor}/aws-sdk-cpp/lib/libaws-cpp-sdk-core.a
      -Daws-cpp-sdk-firehose_library:FILEPATH=#{vendor}/aws-sdk-cpp/lib/libaws-cpp-sdk-firehose.a
      -Daws-cpp-sdk-kinesis_library:FILEPATH=#{vendor}/aws-sdk-cpp/lib/libaws-cpp-sdk-kinesis.a
      -Daws-cpp-sdk-sts_library:FILEPATH=#{vendor}/aws-sdk-cpp/lib/libaws-cpp-sdk-sts.a
      -DCMAKE_CXX_FLAGS_RELEASE:STRING=#{cxx_flags_release.join(" ")}
    ]

    (buildpath/"third-party").install resource("third-party")

    system "cmake", ".", *args
    system "make"
    system "make", "install"
    (include/"osquery/core").install Dir["osquery/core/*.h"]
  end

  plist_options startup: true, manual: "osqueryd"

  test do
    assert_match "platform_info", shell_output("#{bin}/osqueryi -L")
  end
end
