/*
	Copyright(c) 2012 Johannes Jordan <johannes.jordan@cs.fau.de>.
	Copyright(c) 2012 Petr Koupy <petr.koupy@gmail.com>

	This file may be licensed under the terms of of the GNU General Public
	License, version 3, as published by the Free Software Foundation. You can
	find it here: http://www.gnu.org/licenses/gpl.html
*/

#ifndef DISTVIEWMODEL_H
#define DISTVIEWMODEL_H

#include "../model/representation.h"
#include "compute.h"

#include <multi_img.h>

#include <vector>
#include <map>

class BackgroundTaskQueue;

class DistViewModel : public QObject {
    Q_OBJECT
public:
	DistViewModel(representation::t type);

	std::pair<multi_img::Value, multi_img::Value> getRange();
	QPolygonF getPixelOverlay(int y, int x);

	void setTaskQueue(BackgroundTaskQueue *q) { queue = q; }
	void setContext(vpctx_ptr ctx) { context = ctx; }
	void setBinSets(sets_ptr sets) { binsets = sets; }

	// highlight mask for overlays
	const cv::Mat1b& getHighlightMask() { return highlightMask; }
	void clearMask();
	void fillMaskSingle(int dim, int sel);
	void fillMaskLimiters(const std::vector<std::pair<int, int> >& limits);
	void updateMaskLimiters(const std::vector<std::pair<int, int> >&, int dim);

public slots:
	void setLabelColors(QVector<QColor> colors);
	void setIlluminant(cv::Mat1f illum);

	/* remove and add single pixels. currently not in use.
	 * we use updateLabelsPartially instead
	 */
	void subPixels(const std::map<std::pair<int, int>, short> &points);
	void addPixels(const std::map<std::pair<int, int>, short> &points);

	void subImage(sets_ptr temp, const std::vector<cv::Rect> &regions, cv::Rect roi);
	void addImage(sets_ptr temp, const std::vector<cv::Rect> &regions, cv::Rect roi);
	void setImage(SharedMultiImgPtr image, cv::Rect roi, int bins);

	void updateBinning(int bins);

	void updateLabels(const cv::Mat1s& labels,
					  const QVector<QColor> &colors = QVector<QColor>());
	void updateLabelsPartially(const cv::Mat1s &labels, const cv::Mat1b &mask);

	void toggleLabels(bool toggle);

	// glue functions to append type
	void propagateBinning(bool updated);
	void propagateBinningRange(bool updated);

signals:
	void newBinning(representation::t type);
	void newBinningRange(representation::t type);

protected:

	representation::t type;
	SharedMultiImgPtr image;
	cv::Mat1s labels;
	vpctx_ptr context;
	sets_ptr binsets;
	BackgroundTaskQueue *queue;

	QVector<QColor> labelColors;
	std::vector<multi_img::Value> illuminant;
	bool ignoreLabels;
	cv::Mat1b highlightMask;
};

#endif // DISTVIEWMODEL
