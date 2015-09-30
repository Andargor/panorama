// File: dog.cc
// Date: Thu Jul 04 11:51:48 2013 +0800
// Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#include "lib/config.hh"
#include "dog.hh"
#include "lib/utils.hh"
#include "lib/imgproc.hh"
#include "lib/timer.hh"
#include "filter.hh"
using namespace std;

Octave::Octave(const Mat32f& m, int num_scale):
	nscale(num_scale),
	data(num_scale), mag(num_scale), ort(num_scale),
	w(m.width()), h(m.height())
{
	TotalTimer tm("init_octave");
	if (m.channels() == 3)
		data[0] = rgb2grey(m);
	else
		data[0] = m.clone();

	Filter blurer(nscale, GAUSS_SIGMA, SCALE_FACTOR);
	for (int i = 1; i < nscale; i ++) {
		data[i] = blurer.GaussianBlur(data[0], i);	// sigma needs a better one
		cal_mag_ort(i);
	}
}

void Octave::cal_mag_ort(int i) {
	TotalTimer tm("cal_mag_ort");
	const Mat32f& orig = data[i];
	int w = orig.width(), h = orig.height();
	mag[i] = Mat32f(h, w, 1);
	ort[i] = Mat32f(h, w, 1);
	REP(y, h) {
		float *mag_row = mag[i].ptr(y),
					*ort_row = ort[i].ptr(y);
		const float *orig_row = orig.ptr(y),
								*orig_plus = orig.ptr(y + 1),
								*orig_minus = orig.ptr(y - 1);
		// x == 0:
		mag_row[0] = 0;
		ort_row[0] = M_PI;
		REPL(x, 1, w-1) {
			if (between(y, 1, h - 1)) {
				float dy = orig_plus[x] - orig_minus[x],
							 dx = orig_row[x + 1] - orig_row[x - 1];
				mag_row[x] = hypotf(dx, dy);
				if (dx == 0 && dy == 0)
					ort_row[x] = M_PI;
				else
					ort_row[x] = atan2(dy, dx) + M_PI;
			} else {
				mag_row[x] = 0;
				ort_row[x] = M_PI;
			}
		}
		// x == w-1
		mag_row[w-1] = 0;
		ort_row[w-1] = M_PI;

	}
}

ScaleSpace::ScaleSpace(const Mat32f& mat, int num_octave, int num_scale):
	noctave(num_octave), nscale(num_scale),
	origw(mat.width()), origh(mat.height())
{
		GuardedTimer tm("Building scale space");
		// #pragma omp parallel for schedule(dynamic)
		REP(i, noctave) {
			if (!i)
				octaves.emplace_back(mat, nscale);
			else {
				float factor = pow(SCALE_FACTOR, -i);
				int neww = ceil(origw * factor),
						newh = ceil(origh * factor);
				Mat32f resized(newh, neww, 3);
				resize(mat, resized);
				octaves.emplace_back(resized, nscale);
			}
		}
	}

DOG::DOG(const Octave& o):
	nscale(o.get_len()),
	data(nscale - 1) {
		REP(i, nscale - 1)
			data[i] = diff(o.get(i), o.get(i + 1));
	}

Mat32f DOG::diff(const Mat32f& img1, const Mat32f& img2) {
	int w = img1.width(), h = img1.height();
	m_assert(w == img2.width() && h == img2.height());
	Mat32f ret(h, w, 1);
	REP(i, h) REP(j, w) {
		float diff = fabs(img1.at(i, j) - img2.at(i, j));
		ret.at(i, j) = diff;
	}
	return ret;
}

DOGSpace::DOGSpace(ScaleSpace& ss):
	noctave(ss.noctave), nscale(ss.nscale),
	origw(ss.origw), origh(ss.origh),
	dogs(noctave)
{
#pragma omp parallel for schedule(dynamic)
		REP(i, noctave)
			dogs[i].reset(new DOG(ss.octaves[i]));
}
