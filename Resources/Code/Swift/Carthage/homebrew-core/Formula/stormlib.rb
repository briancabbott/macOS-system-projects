class Stormlib < Formula
  desc "Library for handling Blizzard MPQ archives"
  homepage "http://www.zezula.net/en/mpq/stormlib.html"
  url "https://github.com/ladislav-zezula/StormLib/archive/v9.23.tar.gz"
  sha256 "d62ba42f1e02efcb2cbaa03bd2e20fbd18c45499ef5fe65ffb89ee52a7bd9c92"
  license "MIT"
  head "https://github.com/ladislav-zezula/StormLib.git"

  bottle do
    cellar :any
    sha256 "12177d76e3bac8c67baba52c812a855642e780624d7a75f1e826b10811de35b4" => :big_sur
    sha256 "16d13a201008b0f6c145e80d28ced76f29af97dfcfce05d1bc2dac84ac0dba33" => :arm64_big_sur
    sha256 "686a27d3793a4a80858f442d1feda9d5880e21e687c152067136b4bb27c6fa50" => :catalina
    sha256 "0270b8a31bf89afd8a81a0b8e36f3a967e196f024a3900fdf24ef5ab1b26a422" => :mojave
  end

  depends_on "cmake" => :build

  uses_from_macos "bzip2"
  uses_from_macos "zlib"

  # prevents cmake from trying to write to /Library/Frameworks/
  patch :DATA

  def install
    # remove in the next release
    inreplace "src/StormLib.h", "dwCompressionNext = MPQ_COMPRESSION_NEXT_SAME",
                                  "dwCompressionNext"

    system "cmake", ".", *std_cmake_args
    system "make", "install"
    system "cmake", ".", "-DBUILD_SHARED_LIBS=ON", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <stdio.h>
      #include <StormLib.h>

      int main(int argc, char *argv[]) {
        printf("%s", STORMLIB_VERSION_STRING);
        return 0;
      }
    EOS
    system ENV.cc, "-o", "test", "test.c"
    assert_equal version.to_s, shell_output("./test")
  end
end

__END__
diff --git a/CMakeLists.txt b/CMakeLists.txt
index bd8d336..927a47d 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -314,7 +314,6 @@ if(BUILD_SHARED_LIBS)
     message(STATUS "Linking against dependent libraries dynamically")

     if(APPLE)
-        set_target_properties(${LIBRARY_NAME} PROPERTIES FRAMEWORK true)
         set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "-framework Carbon")
     endif()
     if(UNIX)
diff --git a/src/StormLib.h b/src/StormLib.h
index f254290..43fefb8 100644
--- a/src/StormLib.h
+++ b/src/StormLib.h
@@ -480,7 +480,9 @@ typedef void (WINAPI * SFILE_ADDFILE_CALLBACK)(void * pvUserData, DWORD dwBytesW
 typedef void (WINAPI * SFILE_COMPACT_CALLBACK)(void * pvUserData, DWORD dwWorkType, ULONGLONG BytesProcessed, ULONGLONG TotalBytes);

 struct TFileStream;
+typedef struct TFileStream TFileStream;
 struct TMPQBits;
+typedef struct TMPQBits TMPQBits;
