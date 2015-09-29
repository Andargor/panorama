//File: imgproc.hh
//Date:
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include "mat.h"
#include "color.hh"

#include <list>

Mat32f read_rgb(const char* fname);

Mat32f hconcat(const std::list<Mat32f>& mats);

Color interpolate(const Mat32f& mat, float r, float c);

void fill(Mat32f& mat, const Color& c);

template <typename T>
void resize(const Mat<T> &src, Mat<T> &dst);
