class Swftools < Formula
  desc "SWF manipulation and generation tools"
  homepage "http://www.swftools.org/"
  url "http://www.swftools.org/swftools-0.9.2.tar.gz"
  sha256 "bf6891bfc6bf535a1a99a485478f7896ebacbe3bbf545ba551298080a26f01f1"
  revision 1

  livecheck do
    url "http://www.swftools.org/download.html"
    regex(/href=.*?swftools[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    rebuild 1
    sha256 "bacf30e9986bb179127942abea49fac9ca05cf1ac3b3851cf3faf1cb970009b4" => :big_sur
    sha256 "4737739b57d119d07cca0689481151e9ed2a815d3e026d85be4354ef76200877" => :arm64_big_sur
    sha256 "b0791e6725e6d07610847df7e4431e5839fcf72120cea34f1890b425f8e024c4" => :catalina
    sha256 "bf18bfc66b1f6d6ed247acd0a4208a09b4acf6a4668e8f7eba2e40ad33ffe9f6" => :mojave
    sha256 "d0e441ed7eef07c3536965d5269f648744ceb62d41fbcfe9a12248b8154c4f62" => :high_sierra
  end

  # Fixes a conftest for libfftwf.dylib that mistakenly calls fftw_malloc()
  # rather than fftwf_malloc().  Reported upstream to their mailing list:
  # https://lists.nongnu.org/archive/html/swftools-common/2012-04/msg00014.html
  # Patch is merged upstream.  Remove at swftools-0.9.3.
  patch :DATA

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end

  test do
    system "#{bin}/png2swf", "swftools_test.swf", test_fixtures("test.png")
  end
end

__END__
--- a/configure	2012-04-08 10:25:35.000000000 -0700
+++ b/configure	2012-04-09 17:42:10.000000000 -0700
@@ -6243,7 +6243,7 @@

     int main()
     {
-	char*data = fftw_malloc(sizeof(fftwf_complex)*600*800);
+	char*data = fftwf_malloc(sizeof(fftwf_complex)*600*800);
     	fftwf_plan plan = fftwf_plan_dft_2d(600, 800, (fftwf_complex*)data, (fftwf_complex*)data, FFTW_FORWARD, FFTW_ESTIMATE);
 	plan = fftwf_plan_dft_r2c_2d(600, 800, (float*)data, (fftwf_complex*)data, FFTW_ESTIMATE);
 	plan = fftwf_plan_dft_c2r_2d(600, 800, (fftwf_complex*)data, (float*)data, FFTW_ESTIMATE);
