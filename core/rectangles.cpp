#include <vector>
#include <opencv2/core/core.hpp>
#include "rectangles.h"

int rectComplement(int width, int height, cv::Rect r,
				   std::vector<cv::Rect> &result)
{
	int area = 0;

	auto addRect = [&](int x, int y, int w, int h) {
		if (w <= 0 || h <= 0) // don't add empty rectangles
			return;
		cv::Rect rect(x, y, w, h);
		area += rect.area();
		result.push_back(rect);
	};

	addRect(0, 0, r.x, r.y);
	addRect(r.x, 0, r.width, r.y);
	addRect(r.x + r.width, 0, width - r.width - r.x, r.y);
	addRect(0, r.y, r.x, r.height);
	addRect(r.x + r.width, r.y, width - r.width - r.x, r.height);
	addRect(0, r.y + r.height, r.x, height - r.height - r.y);
	addRect(r.x, r.y + r.height, r.width, height - r.height - r.y);
	addRect(r.x + r.width, r.y + r.height,
	        width - r.width - r.x, height - r.height - r.y);

	return area;
}

bool rectTransform(const cv::Rect &oldR, const cv::Rect &newR,
				   std::vector<cv::Rect> &sub,
				   std::vector<cv::Rect> &add)
{
	// intersection between old and new on global coordinates
	cv::Rect isecGlob = oldR & newR;
	// intersection in coordinates relative to oldRoi
	cv::Rect isecOld(0, 0, 0, 0);
	// intersection in coordinates relative to newRoi
	cv::Rect isecNew(0, 0, 0, 0);
	if (isecGlob.width > 0 && isecGlob.height > 0) {
		isecOld.x = isecGlob.x - oldR.x;
		isecOld.y = isecGlob.y - oldR.y;
		isecOld.width = isecGlob.width;
		isecOld.height = isecGlob.height;

		isecNew.x = isecGlob.x - newR.x;
		isecNew.y = isecGlob.y - newR.y;
		isecNew.width = isecGlob.width;
		isecNew.height = isecGlob.height;
	}

	int subArea, addArea;

	// store areas in provided rectangle vectors
	subArea = rectComplement(
		oldR.width, oldR.height, isecOld, sub);

	addArea = rectComplement(
		newR.width, newR.height, isecNew, add);

	// compare amount of pixels for changed area and new area
	return ((subArea + addArea) < (newR.width * newR.height));
}
