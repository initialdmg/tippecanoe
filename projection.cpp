#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <atomic>
#include "projection.hpp"

struct projection projections[] = {
	{"EPSG:4490", lonlat2tile, tile2lonlat, "urn:ogc:def:crs:EPSG::4490"},
	{"EPSG:4087", epsg4087totile, tiletoepsg4087, "urn:ogc:def:crs:EPSG::4087"},
	{NULL, NULL, NULL, NULL},
};

struct projection *projection = &projections[0];

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void lonlat2tile(double lon, double lat, int zoom, long long *x, long long *y) {
	// Place infinite and NaN coordinates off the edge of the Mercator plane

	int lat_class = fpclassify(lat);
	int lon_class = fpclassify(lon);

	if (lat_class == FP_INFINITE || lat_class == FP_NAN) {
		lat = 90;
	}
	if (lon_class == FP_INFINITE || lon_class == FP_NAN) {
		lon = 360;
	}

	// Must limit latitude somewhere to prevent overflow.
	// 89.9 degrees latitude is 0.621 worlds beyond the edge of the flat earth,
	// hopefully far enough out that there are few expectations about the shape.
	if (lat < -90) {
		lat = -90;
	}
	if (lat > 90) {
		lat = 90;
	}

	if (lon < -360) {
		lon = -360;
	}
	if (lon > 360) {
		lon = 360;
	}

	unsigned long long n = 1LL << zoom;

	long long llx = n * ((lon + 180) / 360);
	long long lly = n * ((90 - lat) / 360);

	*x = llx;
	*y = lly;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2lonlat(long long x, long long y, int zoom, double *lon, double *lat) {
	unsigned long long n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	*lat = 90.0 - 360.0 * y / n;
}

void epsg4087totile(double ix, double iy, int zoom, long long *x, long long *y) {
	// Place infinite and NaN coordinates off the edge of the Mercator plane

	int iy_class = fpclassify(iy);
	int ix_class = fpclassify(ix);

	if (iy_class == FP_INFINITE || iy_class == FP_NAN) {
		iy = 40000000.0;
	}
	if (ix_class == FP_INFINITE || ix_class == FP_NAN) {
		ix = 40000000.0;
	}

	*x = (ix / (2 * M_PI * 6378137.0) + 0.5) * (1LL << zoom);
	*y = (0.25 - iy / (2 * M_PI * 6378137.0)) * (1LL << zoom);
}

void tiletoepsg4087(long long ix, long long iy, int zoom, double *ox, double *oy) {
	*ox = (2.0 * ix / (1LL << zoom) - 1) * M_PI * 6378137.0;
	*oy = (0.5 - 2.0 * iy / (1LL << zoom)) * M_PI * 6378137.0;
}

unsigned long long encode(unsigned int wx, unsigned int wy) {
	unsigned long long out = 0;

	int i;
	for (i = 0; i < 32; i++) {
		unsigned long long v = ((wx >> (32 - (i + 1))) & 1) << 1;
		v |= (wy >> (32 - (i + 1))) & 1;
		v = v << (64 - 2 * (i + 1));

		out |= v;
	}

	return out;
}

static std::atomic<unsigned char> decodex[256];
static std::atomic<unsigned char> decodey[256];

void decode(unsigned long long index, unsigned *wx, unsigned *wy) {
	static std::atomic<int> initialized(0);
	if (!initialized) {
		for (size_t ix = 0; ix < 256; ix++) {
			size_t xx = 0, yy = 0;

			for (size_t i = 0; i < 32; i++) {
				xx |= ((ix >> (64 - 2 * (i + 1) + 1)) & 1) << (32 - (i + 1));
				yy |= ((ix >> (64 - 2 * (i + 1) + 0)) & 1) << (32 - (i + 1));
			}

			decodex[ix] = xx;
			decodey[ix] = yy;
		}

		initialized = 1;
	}

	*wx = *wy = 0;

	for (size_t i = 0; i < 8; i++) {
		*wx |= ((unsigned) decodex[(index >> (8 * i)) & 0xFF]) << (4 * i);
		*wy |= ((unsigned) decodey[(index >> (8 * i)) & 0xFF]) << (4 * i);
	}
}

void set_projection_or_exit(const char *optarg) {
	struct projection *p;
	for (p = projections; p->name != NULL; p++) {
		if (strcmp(p->name, optarg) == 0) {
			projection = p;
			break;
		}
		if (strcmp(p->alias, optarg) == 0) {
			projection = p;
			break;
		}
	}
	if (p->name == NULL) {
		fprintf(stderr, "Unknown projection (-s): %s\n", optarg);
		exit(EXIT_FAILURE);
	}
}
