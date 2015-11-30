//File: ba_common.hh
//Date:
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include <array>
#include "camera.hh"
#include "lib/timer.hh"

namespace {
	const int NR_PARAM_PER_CAMERA = 6;
	const int NR_TERM_PER_MATCH = 2;
	const bool SYMBOLIC_DIFF = true;
	const double LM_lambda = 0.05;
	const int LM_MAX_ITER = 100;

	void camera_to_params(const stitch::Camera& c, double* ptr) {
		ptr[0] = c.focal;
		ptr[1] = c.ppx;
		ptr[2] = c.ppy;
		stitch::Camera::rotation_to_angle(c.R, ptr[3], ptr[4], ptr[5]);
	}

	void params_to_camera(const double* ptr, stitch::Camera& c) {
		c.focal = ptr[0];
		c.ppx = ptr[1];
		c.ppy = ptr[2];
		c.aspect = 1;	// keep it 1
		stitch::Camera::angle_to_rotation(ptr[3], ptr[4], ptr[5], c.R);
	}
}
