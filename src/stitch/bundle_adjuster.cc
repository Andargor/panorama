//File: bundle_adjuster.cc
//Author: Yuxin Wu <ppwwyyxx@gmail.com>

#include "bundle_adjuster.hh"
#include "lib/timer.hh"
#include <cmath>
#include <Eigen/Dense>
using namespace std;

// TODO incremental bundle adjustment, add in the order of confidence
// TODO reject some connections if it breaks the bundle

const static int NR_PARAM_PER_IMAGE = 6;
const static int NR_TERM_PER_MATCH = 2;
const double LM_lambda = 0.05;
const int LM_MAX_ITER = 100;

namespace {
	void camera_to_params(
			const vector<Camera>& cameras, vector<double>& params) {
		REP(i, cameras.size()) {
			auto& c = cameras[i];
			int start = i * NR_PARAM_PER_IMAGE;
			double* ptr = params.data() + start;
			ptr[0] = c.focal;
			ptr[1] = c.ppx;
			ptr[2] = c.ppy;
			Camera::rotation_to_angle(c.R, ptr[3], ptr[4], ptr[5]);
		}
	}

	void params_to_camera(
			const vector<double>& params, vector<Camera>& cameras) {
		REP(i, cameras.size()) {
			auto& c = cameras[i];
			int start = i * NR_PARAM_PER_IMAGE;
			const double* ptr = params.data() + start;
			c.focal = ptr[0];
			c.ppx = ptr[1];
			c.ppy = ptr[2];
			c.aspect = 1;	// keep it 1
			Camera::angle_to_rotation(ptr[3], ptr[4], ptr[5], c.R);
		}
	}
}

BundleAdjuster::BundleAdjuster(const vector<Mat32f>& imgs,
		const vector<vector<MatchInfo>>& pairwise_matches):
	imgs(imgs), pairwise_matches(pairwise_matches),
	nr_img(pairwise_matches.size()),
	nr_match(0),
	params(pairwise_matches.size() * NR_PARAM_PER_IMAGE)
	{
		REP(i, nr_img)
			REPL(j, i+1, nr_img) {
				auto& m = pairwise_matches[j][i];
				nr_match += m.match.size();
			}
	}

bool BundleAdjuster::estimate(std::vector<Camera>& cameras) {
	using namespace Eigen;
	GuardedTimer tm("BundleAdjuster::estimate()");
	m_assert((int)cameras.size() == nr_img);
	camera_to_params(cameras, params);
	vector<double> err(NR_TERM_PER_MATCH * nr_match);
	double best_err = calcError(params, err);
	print_debug("BA: init err: %lf\n", best_err);

	int itr = 0;
	int nr_non_decrease = 0;	// number of non-decreasing iteration
	while (itr++ < LM_MAX_ITER) {
		Eigen::MatrixXd J(NR_TERM_PER_MATCH * nr_match, NR_PARAM_PER_IMAGE * nr_img);
		calcJacobian(J);
		Eigen::MatrixXd JtJ = J.transpose() * J;
		REP(i, nr_img * NR_PARAM_PER_IMAGE)
			JtJ(i,i) += LM_lambda;	// TODO use different lambda for different param?
		Eigen::Map<VectorXd> err_vec(err.data(), NR_TERM_PER_MATCH * nr_match);
		auto b = J.transpose() * err_vec;
		VectorXd ans = JtJ.jacobiSvd(ComputeThinU | ComputeThinV).solve(b);

		vector<double> newparams = params;
		REP(i, newparams.size())
			newparams[i] += ans(i);
		double now_err = calcError(newparams, err);

		double max = -1e9; for (auto& e : err) update_max(max, abs(e));
		print_debug("BA: average err: %lf, max: %lf\n", now_err, max);
		if (now_err >= best_err - EPS)
			nr_non_decrease ++;
		else {
			nr_non_decrease = 0;
			best_err = now_err;
			params = move(newparams);
		}
		if (nr_non_decrease > 5)
			break;
	}
	print_debug("BA: Error %lf after %d iterations\n", best_err, itr);
	params_to_camera(params, cameras);
	return true;
}

double BundleAdjuster::calcError(
		const vector<double>& params, std::vector<double>& err) {
	m_assert((int)err.size() == NR_TERM_PER_MATCH * nr_match);
	vector<Camera> now_camera(nr_img);
	params_to_camera(params, now_camera);
	int idx = 0;
	REP(i, nr_img) REPL(j, i+1, nr_img) {
		auto& m = pairwise_matches[j][i];
		auto& c_from = now_camera[i], &c_to = now_camera[j];
		Homography Hto_to_from = (c_from.K() * c_from.R.transpose()) * (c_to.R * c_to.K().inverse());
		for (auto& p : m.match)	{
			Vec2D to = p.first, from = p.second;
			Vec2D transformed = Hto_to_from.trans2d(to + Vec2D(imgs[i].width() / 2, imgs[i].height() / 2));
			err[idx] = from.x - transformed.x + imgs[i].width() / 2;
			err[idx+1] = from.y - transformed.y + imgs[i].height() / 2;
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
	const double step = 1e-5;
	vector<double> err1(NR_TERM_PER_MATCH * nr_match);
	vector<double> err2(NR_TERM_PER_MATCH * nr_match);
	REP(i, nr_img) {
		REP(p, NR_PARAM_PER_IMAGE) {
			int param_idx = i * NR_PARAM_PER_IMAGE + p;
			double val = params[param_idx];
			params[param_idx] = val + step;
			calcError(params, err1);
			params[param_idx] = val - step;
			calcError(params, err2);
			params[param_idx] = val;

			// deriv
			REP(k, err1.size())
				J(k, param_idx) = (err2[k] - err1[k]) / (2 * step);
		}
	}
}

HomoEstimator::HomoEstimator(const MatchInfo& m):
	match(m), nr_match(m.match.size()) {}

bool HomoEstimator::estimate(Homography& m) {
	using namespace Eigen;
	params.resize(8);
	REP(i, 8) params[i] = m.ptr()[i];
	vector<double> err(nr_match * 2);
	double prev_err = calcError(err);
	print_debug("init err: %lf\n", prev_err);
	int cnt = 10;
	while (cnt--) {
		Eigen::MatrixXd J(2 * nr_match, 8);
		calcJacobian(J);
		Eigen::MatrixXd JtJ = J.transpose() * J;
		REP(i, 8)
			JtJ(i,i) += 1;
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
	}
	REP(i, 8) m.ptr()[i] = params[i];
	m.ptr()[8] =1;
	return true;
}

double HomoEstimator::calcError(vector<double>& err) {
	m_assert((int)err.size() == NR_TERM_PER_MATCH * nr_match);
	Homography m;
	REP(i, 8) m.ptr()[i] = params[i];
	m.ptr()[8] =1;
	int idx = 0;
	for (auto& p : match.match)	{
		Vec2D to = p.first, from = p.second;
		Vec2D transformed = m.trans2d(from);
		err[idx] = to.x - transformed.x;
		err[idx+1] = to.y - transformed.y;
		idx += 2;
	}
	double sum = 0;
	for (auto& e : err) sum += sqr(e);
	sum /= err.size();
	sum = sqrt(sum);
	return sum;
}

void HomoEstimator::calcJacobian(Eigen::MatrixXd& J) {
	const double step = 2e-5;
	vector<double> err1(NR_TERM_PER_MATCH * nr_match);
	vector<double> err2(NR_TERM_PER_MATCH * nr_match);
	REP(p, 8) {
		int param_idx = p;
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
