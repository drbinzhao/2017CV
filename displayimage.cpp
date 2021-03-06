# include "opencv2/opencv.hpp"
# include <algorithm>
# include <iostream>
# include <fstream>
# include "stdlib.h"
# include "networktables/NetworkTable.h"
# ifdef JETSON_ENV
# include <lidarlite.h>
# endif
# include <chrono>
# include <thread>

using namespace cv;
using namespace std;

//ifdefs are for removing code based on whether the jetson or a desktop is running it

#ifdef DESKTOP_ENV
void on_trackbar(int, void*)
{

}
#endif

int saturation_min;
int saturation_max;
int hue_min;
int hue_max;
int value_min;
int value_max;

int main()
{
	// open video stream
	VideoCapture cap(0); // open the default camera
	if(!cap.isOpened())	// check if we succeeded
			return -1;

	cap.set(V4L2_CID_ISO_SENSITIVITY_AUTO, V4L2_CID_ISO_SENSITIVITY_MANUAL);
	cap.set(V4L2_CID_ISO_SENSITIVITY, 80);
	// Doesnt work do to an old version of v4l
	// cap.set(CV_CAP_PROP_FRAME_WIDTH, 1280);
	// cap.set(CV_CAP_PROP_FRAME_HEIGHT, 720);

#ifdef JETSON_ENV
	auto table = NetworkTable::GetTable("JetsonData");
	LidarLite *lidarLite = new LidarLite();
	int err = lidarLite->openLidarLite();
#endif

	Mat edges;
	Mat dest;

  ifstream hsvValues;
  hsvValues.open("goodvalues.txt");
  string temp;

  getline(hsvValues, temp);
  hue_min = stoi(temp);
  getline(hsvValues, temp);
  hue_max = stoi(temp);
  getline(hsvValues, temp);
  saturation_min = stoi(temp);
  getline(hsvValues, temp);
  saturation_max = stoi(temp);
  getline(hsvValues, temp);
  value_min = stoi(temp);
  getline(hsvValues, temp);
  value_max = stoi(temp);

  hsvValues.close();

#ifdef DESKTOP_ENV
	// create window
	namedWindow("Trackbar",1);

	createTrackbar("Hue Min", "Trackbar", &hue_min, 180, on_trackbar );
	createTrackbar("Hue Max", "Trackbar", &hue_max, 180, on_trackbar );
	createTrackbar("Saturation Min", "Trackbar", &saturation_min, 255, on_trackbar);
	createTrackbar("Saturation Max", "Trackbar", &saturation_max, 255, on_trackbar);
	createTrackbar("Value Min", "Trackbar", &value_min, 255, on_trackbar );
	createTrackbar("Value Max", "Trackbar", &value_max, 255, on_trackbar );
#endif

	for(;;)
	{
		Mat frame;
		cap >> frame; // get a new frame from camera 640x480
		cvtColor(frame, edges, CV_BGR2HSV);
		// GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);

		// make all green colors white and everything else black
		inRange(edges, Scalar(hue_min, saturation_min, value_min),
									 Scalar(hue_max, saturation_max, value_max), dest);

		// creates kernels for erode and dilate
		Mat element_erode = getStructuringElement(MORPH_RECT, Size(10,10), Point(0,0));
		Mat element_dilate = getStructuringElement(MORPH_RECT, Size(5,5), Point(0,0));

		erode(dest, dest, element_erode);
		dilate(dest, dest, element_dilate);

		// find contours
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;

#ifdef DESKTOP_ENV
		imshow("test", dest);
#endif

		findContours(dest, contours, hierarchy, CV_RETR_EXTERNAL,
			CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

		vector<vector<Point> > contours_poly(contours.size());
		vector<Rect> boundingRects;
		boundingRects.reserve(contours.size());

		for(int i = 0; i < contours.size(); ++i)
		{
			approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
			Rect rect = boundingRect(Mat(contours_poly[i]));

			const auto size = rect.size();
			// Filter out invalid rectangles
			const int width = size.width;
			const int height = size.height;
			if (width < 40 || height < 10) {
				continue;
			}

			if (height * 2  > width * 1) {
				continue;
			}

#ifdef DESKTOP_ENV
			static const Scalar color(255,0,0);
			rectangle(frame, rect.tl(), rect.br(), color, 2, 8, 0);
#endif
			// rectangle is acceptable, add it to the list
			boundingRects.emplace_back(std::move(rect));
		}

		sort(boundingRects.begin(), boundingRects.end(), [](const Rect& rect1, const Rect& rect2) -> bool
		{
			return rect1.area() > rect2.area();
		});

#ifdef JETSON_ENV
		if (err >= 0)
		{
			int distance = lidarLite->getDistance();
			if (distance < 0)
			{
				int llerror;
				llerror = lidarLite->getError();
				cout << llerror;
			}
			table->PutNumber("distance", distance);
		}
		if(boundingRects.size() > 0)
		{
			int x_top = (boundingRects[0].tl().x + boundingRects[0].br().x) / 2;
			int y_top = (boundingRects[0].tl().y + boundingRects[0].br().y) / 2;
			cout << x_top;
			// int x_bot = (boundingRects[1].tl().x + boundingRects[1].br().x) / 2;
			// int y_bot = (boundingRects[1].tl().y + boundingRects[1].br().y) / 2;
			table->PutNumber("pixels", x_top);
		}
#endif
#ifdef DESKTOP_ENV
		// show original video stream and edited video stream
		imshow("color", frame);
		if(waitKey(30) >= 0) {
      ofstream hsvValues;
      hsvValues.open("goodvalues.txt");
      hsvValues << hue_min << "\n" << hue_max << "\n" << saturation_min << "\n"
             << saturation_max << "\n" << value_min << "\n" << value_max;
      hsvValues.close();
			break;
		}
#endif
	}
	// the camera will be deinitialized automatically in VideoCapture destructor
	return 0;
}
