//File: bundle_adjuster.cc
//Author: Yuxin Wu <ppwwyyxx@gmail.com>

#include "bundle_adjuster.hh"
#include "lib/timer.hh"
#include <cmath>
#include <Eigen/Dense>
using namespace std;

const int NR_PARAM_PER_IMAGE = 7;
const int NR_TERM_PER_MATCH = 2;

namespace {
	void camera_to_params(const vector<Camera>& cameras,
			vector<double>& params) {
		REP(i, cameras.size()) {
			auto& c = cameras[i];
			int start = i * NR_PARAM_PER_IMAGE;
			double* ptr = params.data() + start;
			ptr[0] = c.focal;
			ptr[1] = c.ppx;
			ptr[2] = c.ppy;
			ptr[3] = c.aspect;
			Camera::rotation_to_angle(c.R, ptr[4], ptr[5], ptr[6]);
		}
	}

	void param_to_camera(const vector<double>& params, vector<Camera>& cameras) {
		REP(i, cameras.size()) {
			auto& c = cameras[i];
			int start = i * NR_PARAM_PER_IMAGE;
			const double* ptr = params.data() + start;
			c.focal = ptr[0];
			c.ppx = ptr[1];
			c.ppy = ptr[2];
			c.aspect = ptr[3];
			Camera::angle_to_rotation(ptr[4], ptr[5], ptr[6], c.R);
		}
	}
}

BundleAdjuster::BundleAdjuster(
		const std::vector<Mat32f>& imgs,
		const std::vector<std::vector<MatchInfo>>& pairwise_matches):
	imgs(imgs), pairwise_matches(pairwise_matches),
	nr_img(imgs.size()),
	nr_match(0),
	params(imgs.size() * NR_PARAM_PER_IMAGE)
	{
		REP(i, nr_img)
			REPL(j, i+1, nr_img) {
				auto& m = pairwise_matches[j][i];
				nr_match += m.match.size();
			}
	}

bool BundleAdjuster::estimate(std::vector<Camera>& cameras) {
	using namespace Eigen;
	m_assert((int)cameras.size() == nr_img);
	camera_to_params(cameras, params);
	vector<double> err(NR_TERM_PER_MATCH * nr_match);
	double prev_err = calcError(err);
	print_debug("init err: %lf\n", prev_err);
	int cnt = 300;
	while (cnt--) {
		if (cnt % 100 == 0)
			print_debug("ba iter=%d\n", 300-cnt);
		Eigen::MatrixXd J(NR_TERM_PER_MATCH * nr_match, NR_PARAM_PER_IMAGE * nr_img);
		calcJacobian(J);
		Eigen::MatrixXd JtJ = J.transpose() * J;
		REP(i, nr_img) {
			JtJ(i,i) += 0.1;
		}
		VectorXd err_vec(NR_TERM_PER_MATCH * nr_match);
		REP(i, err.size()) err_vec(i) = err[i];
		auto b = J.transpose() * err_vec;
		VectorXd ans = JtJ.jacobiSvd(ComputeThinU | ComputeThinV).solve(b);
		REP(i, params.size())
			params[i] += ans(i);

		double now_err = calcError(err);
		double max = -1e9;
		for (auto& e : err)
			update_max(max, abs(e));
		print_debug("average err: %lf, max: %lf\n", now_err, max);
		/*
		 *if (now_err > prev_err - 1e-6)
		 *  break;
		 */
		prev_err = now_err;
	}
	param_to_camera(params, cameras);
	return true;
}

double BundleAdjuster::calcError(std::vector<double>& err) {
	TotalTimer tm("calcError()");
	m_assert((int)err.size() == NR_TERM_PER_MATCH * nr_match);
	vector<Camera> now_camera(nr_img);
	param_to_camera(params, now_camera);
	int idx = 0;
	REP(i, nr_img) REPL(j, i+1, nr_img) {
		auto& m = pairwise_matches[j][i];
		auto& c_from = now_camera[i], &c_to = now_camera[j];
		Homography Hto_to_from = (c_from.K() * c_from.R.transpose()) * (c_to.R * c_to.K().inverse());
		for (auto& p : m.match)	{
			Vec2D to = p.first, from = p.second;
			Vec2D transformed = Hto_to_from.trans2d(to);
			err[idx] = from.x - transformed.x;
			err[idx+1] = from.y - transformed.y;
			idx += 2;
		}
	}
	double sum = 0;
	for (auto& e : err) sum += sqr(e);
	sum /= err.size();
	sum = sqrt(sum);
	return sum;
}

void BundleAdjuster::calcJacobian(Eigen::MatrixXd& J) {
	const double step = 1e-4;
	vector<double> err1(NR_TERM_PER_MATCH * nr_match);
	vector<double> err2(NR_TERM_PER_MATCH * nr_match);
	REP(i, nr_img) {
		REP(p, NR_PARAM_PER_IMAGE) {
			int param_idx = i * NR_PARAM_PER_IMAGE + p;
			double val = params[param_idx];
			params[param_idx] = val + step;
			calcError(err1);
			params[param_idx] = val - step;
			calcError(err2);
			params[param_idx] = val;

			// deriv
			REP(k, err1.size())
				J(k, param_idx) = (err2[k] - err1[k]) / (2 * step);
		}
	}
}
