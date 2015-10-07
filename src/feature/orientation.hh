//File: orientation.hh
//Date:
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include "dog.hh"
#include "feature.hh"


namespace feature {

class OrientationAssign {
	public:
		OrientationAssign(
				const DOGSpace& dog, const ScaleSpace& ss,
				const std::vector<SSPoint>& keypoints);

		// assign orientation to each SSPoint
		std::vector<SSPoint> work() const;

	protected:
		const DOGSpace& dog;
		const ScaleSpace& ss;
		const std::vector<SSPoint>& points;

		std::vector<float> calc_dir(const SSPoint& p) const;
};

}
