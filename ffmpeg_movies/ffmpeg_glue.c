#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <float.h>
#include <fcntl.h>
#include <io.h>

// this glue is necessary to make FFMpeg work with MSVCRT

int strncasecmp(const char *a, const char *b, size_t n) { return _strnicmp(a, b, n); }
int strcasecmp(const char *a, const char *b) { return _stricmp(a, b); }
int usleep(int t) { Sleep(t / 1000); return 0; }

// ffmpeg needs these symbols, but they're not really used
void gettimeofday() {}
void snprintf() {}

int *__errno()
{
	return _errno();
}

#undef sinf
#undef cosf
#undef log10f
#undef powf
#undef truncf
#undef ldexpf
#undef expf
#undef logf
#undef exp2f
float sinf(float a) { return (float)sin(a); }
float cosf(float a) { return (float)cos(a); }
float log10f(float a) { return (float)log10(a); }
float powf(float a, float b) { return (float)pow(a, b); }
float truncf(float a) { return (float)(a > 0 ? floor(a) : ceil(a)); }
float ldexpf(float a, int b) { return (float)ldexp(a, b); }
float expf(float a) { return (float)exp(a); }
float logf(float a) { return (float)log(a); }
#define exp2(x) exp((x) * 0.693147180559945)
float exp2f(float a) { return (float)exp2(a); }

#define FP_NAN         0
#define FP_INFINITE    1
#define FP_ZERO        2
#define FP_SUBNORMAL   3
#define FP_NORMAL      4

int __fpclassifyd(double a)
{
	if(_isnan(a)) return FP_NAN;
	if(!_finite(a)) return FP_INFINITE;
	if(a == 0.0) return FP_ZERO;

	return FP_NORMAL;
}

static float CBRT2 = 1.25992104989487316477f;
static float CBRT4 = 1.58740105196819947475f;

float cbrtf( float xx )
{
int e, rem, sign;
float x, z;

x = xx;
if( x == 0 )
	return( 0.0f );
if( x > 0 )
	sign = 1;
else
	{
	sign = -1;
	x = -x;
	}

z = x;
x = frexpf( x, &e );

x = (((-0.13466110473359520655053f  * x
      + 0.54664601366395524503440f ) * x
      - 0.95438224771509446525043f ) * x
      + 1.1399983354717293273738f  ) * x
      + 0.40238979564544752126924f;

if( e >= 0 )
	{
	rem = e;
	e /= 3;
	rem -= 3*e;
	if( rem == 1 )
		x *= CBRT2;
	else if( rem == 2 )
		x *= CBRT4;
	}

else
	{
	e = -e;
	rem = e;
	e /= 3;
	rem -= 3*e;
	if( rem == 1 )
		x /= CBRT2;
	else if( rem == 2 )
		x /= CBRT4;
	e = -e;
	}

x = ldexpf( x, e );

x -= ( x - (z/(x*x)) ) * 0.333333333333f;

if( sign < 0 )
	x = -x;
return(x);
}

int ffmpeg_open(char *name, int access, int mode)
{
	access &= 0xFFF;

	access |= O_BINARY;

	return open(name, access);
}

int ffmpeg_lseek(int fd, int pos, int whence)
{
	return lseek(fd, pos, whence);
}
