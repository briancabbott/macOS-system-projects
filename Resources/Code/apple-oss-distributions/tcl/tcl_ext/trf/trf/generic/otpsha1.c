/* otpsha1.c - the OTP variant of SHA1 */

/* ideally we would like to define a new argument in dig_opt.c that allows
   us to turn the OTP variant on and off. unfortunately, the variant dictates
   the size of the resulting digest and that's one thing that's compile time.

   hence, the hack du'jour...
 */

#define OTP
#include "sha1.c"
