//plate.h

#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

enum Color {BLUE, YELLOW, UNKNOWN};

void plateColorLocate(Mat src, vector<Mat> &results, Color color);
void plateSobelLocate(Mat src, vector<Mat> &results);
void plateLocate(Mat src, vector<Mat> &results);
bool rotation(Mat &in, Mat &out, const Size rect_size, const Point2f center, const double angle);
void globelRotation(const Mat src, vector<Mat> &results, vector<vector<Point> > contours);
void partialRotation(const Mat src, Mat src_grey, vector<Mat> &results, vector<vector<Point> > contours);
bool calcSafeRect(const RotatedRect& roi_rect, const Mat& src,
	Rect_<float>& safeBoundRect);
void affine(const Mat& in, Mat& out, const double slope);//��ˮƽб���γ��ƣ���ת��������
//! �Ƿ�ƫб
//! �����ֵ��ͼ������жϽ��
bool isdeflection(const Mat& in, const double angle, double& slope);
