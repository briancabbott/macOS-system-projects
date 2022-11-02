class G3log < Formula
  desc "Asynchronous, 'crash safe', logger that is easy to use"
  homepage "https://github.com/KjellKod/g3log"
  url "https://github.com/KjellKod/g3log/archive/1.3.4.tar.gz"
  sha256 "2fe8815e5f5afec6b49bdfedfba1e86b8e58a5dc89fd97f4868fb7f3141aed19"
  license "Unlicense"

  bottle do
    cellar :any
    sha256 "5e24eda970bf16a1d737e0112ef7e86651c6cdd29b14b6dd4beec2faf9f9d292" => :big_sur
    sha256 "d6ab9b85de4f0bc70d278210ac4a89c2780b4a271dc474fdd2a4ac16933a3d38" => :arm64_big_sur
    sha256 "3325a5a22c63c02f6c3a7d9b35f533e579f369ff2871f7152d0ca4994bb049d3" => :catalina
    sha256 "f44e98ef652573827da51288539acb1122af634b79f61f8ec2687b7b5184e971" => :mojave
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS.gsub(/TESTDIR/, testpath)
      #include <g3log/g3log.hpp>
      #include <g3log/logworker.hpp>
      int main()
      {
        using namespace g3;
        auto worker = LogWorker::createLogWorker();
        worker->addDefaultLogger("test", "TESTDIR");
        g3::initializeLogging(worker.get());
        LOG(DEBUG) << "Hello World";
        return 0;
      }
    EOS
    system ENV.cxx, "-std=c++14", "test.cpp", "-L#{lib}", "-lg3log", "-o", "test"
    system "./test"
    Dir.glob(testpath/"test.g3log.*.log").any?
  end
end
