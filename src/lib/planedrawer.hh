// File: planedrawer.hh
// Date: Fri May 03 04:52:58 2013 +0800
// Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include "lib/color.hh"
#include "lib/mat.h"

namespace pano {

typedef std::pair<Coor, Coor> Line2D;
typedef std::vector<Coor> Polygon;

class PlaneDrawer {
	public:
		// will directly WRITE to mat
		PlaneDrawer(Mat32f mat):
			 mat(mat) {
			m_assert(mat.channels() == 3);
		}

		virtual ~PlaneDrawer(){}

		void set_color(Color m_c)
		{ c = m_c; }

		void set_rand_color() {
			auto gen_rand = []() { return (real_t)rand() / RAND_MAX; };
			set_color(Color(gen_rand(), gen_rand(), gen_rand()));
		}

		inline void point(int x, int y) {
			// drawing algorithms are easy to draw out-of-range
			if (!between(x, 0, mat.width()) ||
					!between(y, 0, mat.height()))
				return;
			float* p = mat.ptr(y, x);
			c.write_to(p);
		}

		inline void point(Coor v)
		{ point(v.x, v.y); }

		inline void line(Coor s, Coor t)
		{ Bresenham(s, t); }

		inline void line(Vec2D s, Vec2D t)
		{ line(Coor(s.x, s.y), Coor(t.x, t.y)); }

		inline void line(Line2D l)
		{ line(l.first, l.second); }

		inline void line(std::pair<Vec2D, Vec2D> l)
		{ line(l.first, l.second); }

		void circle(Coor o, int r);

		void cross(Coor o, int r);

		void cross(Vec2D o, int r)
		{ cross(Coor(o.x, o.y), r); }

		void arrow(Coor o, real_t dir, int r);

		void polygon(Polygon p) {
			for (unsigned int i = 0; i < p.size() - 1; i++)
				line(p[i], p[i + 1]);
			line(p.back(), p.front());
		}

		void polygon(std::vector<Vec2D> p)
		{ polygon(to_renderable(p)); }

		//Mat32f& get_img() { return mat; }

	protected:
		void Bresenham(Coor s, Coor t);
		Mat32f mat;
		Color c = Color::BLACK;

		Polygon to_renderable(std::vector<Vec2D> p) {
			Polygon ret;
			for (auto v : p)
				ret.push_back(Coor(v.x, v.y));
			return ret;
		}
};

}
