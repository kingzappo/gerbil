/*	
	Copyright(c) 2012 Petr Koupy <petr.koupy@gmail.com>

	This file may be licensed under the terms of of the GNU General Public
	License, version 3, as published by the Free Software Foundation. You can
	find it here: http://www.gnu.org/licenses/gpl.html
*/

#ifndef BACKGROUND_TASK_H
#define BACKGROUND_TASK_H

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>
#include <opencv2/core/core.hpp>

#ifdef WITH_QT
#include <QObject>
#endif

/** Abstract class for background tasks. Tasks are expected to be queued into
    BackgroundTaskQueue by which they are dispatched one-by-one. Inheritors
	shall implement run() and cancel() functions which are specific to the
	actual algorithm and technology. Algorithm executed in run() function can
	optionally report progress via update() function. Task creator can wait 
	for completion either synchronously or asynchronously. */
#ifdef WITH_QT
class BackgroundTask : public QObject {
	Q_OBJECT
#elif
class BackgroundTask {
#endif

public:
	BackgroundTask() : terminated(false), success(false) {};
	virtual ~BackgroundTask() {};

	/** Task-specific algorithm implemented in the inheritor. It depends
	    on the actual implementation what technologies and what amount
		of parallelism is used for the calculation. */
	virtual void run() = 0;
	/** Task-specific cancellation routine implemented in the inheritor.
	    Since not all technologies support cancellation, it is not guaranteed
		that call to this function actually cancels the task. */
	virtual void cancel() = 0;
	/** Report task progress to all listeners. */
	void update(int percent);
	/** Report task completion to all listeners. Wake up blocked listeners. */
	void done(bool success);
	/** Passivelly and synchronously wait for task completion. Task success is 
	    reported via return value. */
	bool wait();
	/** Retrieve ROI associated with this task. Returned ROI might be empty
	    (zero width and height) if the task is not associated with any ROI. */
	cv::Rect roi();

#ifdef WITH_QT
signals:
	/** Optional progress updates for asynchronous listeners. */
	void progress(std::string &description, int percent);
	/** Compulsory completion notification for asynchronous listeners. */
	void finished(bool success);
#endif

protected:
	typedef boost::mutex Guard;
    typedef boost::unique_lock<Guard> Lock;

	/** Serializes thread access to this->terminated variable. */
	Guard guard; 
	/** Wakes client which is synchronously waiting for task result. */
	boost::condition_variable future; 
	/** Specifies whether the task calculation is already finished
	    (either successfully or cancelled). */
	bool terminated;
	/** Specifies whether the task calculation was finished successfully. */
	bool success;
	/** Short description of task for GUI progress updates. */
	std::string description; 
	/** Either target ROI for this calculation or empty ROI (zero width and height)
	    for non-ROI calculations. */
	cv::Rect targetRoi; 
};

typedef boost::shared_ptr<BackgroundTask> BackgroundTaskPtr;

#endif
