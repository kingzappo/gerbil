#include "graphsegmentation.h"

#include <graphseg_config.h>
#include "../tasks/graphsegbackground.h"

#include <background_task_queue.h>


GraphSegmentationModel::GraphSegmentationModel(QObject *parent,
											   BackgroundTaskQueue *queue)
	: QObject(parent), queue(queue), graphsegResult(new cv::Mat1s()) { }

GraphSegmentationModel::~GraphSegmentationModel() { }

void GraphSegmentationModel::setMultiImage(representation::t type,
										   SharedMultiImgPtr image)
{
	if (type == representation::IMG || type == representation::GRAD)
		map.insert(type, image);
	else // tryed setting an image of an unsupported representation type
		assert(false);
}

void GraphSegmentationModel::setSeedMap(cv::Mat1s *seedMap)
{
	this->seedMap = seedMap;
}

void GraphSegmentationModel::setCurLabel(int curLabel)
{
	this->curLabel = curLabel;
}

void GraphSegmentationModel::setCurBand(representation::t type, int bandId)
{
	if (type == representation::IMG || type == representation::GRAD)
	{
		this->curRepr = type;
		this->curBand = bandId;
	}
}

void GraphSegmentationModel::runGraphseg(representation::t type,
							   const vole::GraphSegConfig &config)
{
	SharedMultiImgPtr input = map.value(type);
	if (!input) // image of type type was not set with setMultiImg
		assert(false);

	startGraphseg(input, config);
}

void GraphSegmentationModel::runGraphsegBand(const vole::GraphSegConfig &config)
{
	SharedMultiImgPtr image = map.value(curRepr);
	if (!image) // image of type type was not set with setMultiImg
		assert(false);

	SharedDataLock img_lock(image->mutex);
	SharedMultiImgPtr input(new SharedMultiImgBase(
		new multi_img((**image)[curBand], (*image)->minval, (*image)->maxval)));
	img_lock.unlock();
	startGraphseg(input, config);
}

void GraphSegmentationModel::startGraphseg(SharedMultiImgPtr input,
										   const vole::GraphSegConfig &config)
{
	emit setGUIEnabledRequested(false, TT_NONE);
	// TODO: should this be a commandrunner instead? arguable..
	BackgroundTaskPtr taskGraphseg(new GraphsegBackground(
		config, input, *seedMap, graphsegResult));
	QObject::connect(taskGraphseg.get(), SIGNAL(finished(bool)),
		this, SLOT(finishGraphSeg(bool)), Qt::QueuedConnection);
	queue->push(taskGraphseg);
}

void GraphSegmentationModel::finishGraphSeg(bool success)
{
	if (success) {
		// add segmentation to current labeling
		emit alterLabelRequested((short)curLabel + 1,
								 *(graphsegResult.get()),
								 false);
		// leave seeding mode for convenience
		emit seedingDone();

		emit setGUIEnabledRequested(true, TT_NONE);
	}
	// TODO: warn user in case of failure?
}