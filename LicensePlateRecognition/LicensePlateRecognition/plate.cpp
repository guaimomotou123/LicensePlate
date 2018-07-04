#include "plate.h"

bool rotation(Mat &in, Mat &out, const Size rect_size, const Point2f center, const double angle)
{
	Mat in_large;
	in_large.create(in.cols * 1.5, in.rows * 1.5, in.type());

	int x = in_large.cols / 2 - center.x > 0 ? in_large.cols / 2 - center.x : 0;//使左右两边留的一样
	int y = in_large.rows / 2 - center.y > 0 ? in_large.rows / 2 - center.y : 0;//是上下两边留的一样

	int width = x + in.cols < in_large.cols ? in.cols : in_large.cols - x;//如果大于，说明in_larger尺寸过小，没法融合
	int height = y + in.rows < in_large.rows ? in.rows : in_large.rows - y;

	//由于addWeighted函数，要求融合的尺寸必须相等，如果不相等，融合发生错误，所以要判断一下
	if (width != in.cols || height != in.rows)
		return false;

	//融合图形，新图形还保存在in_large
	Mat imageRoi = in_large(Rect(x, y, width, height));
	addWeighted(imageRoi, 0, in, 1, 0, imageRoi);

	Point2f center_diff(in.cols / 2, in.rows / 2);
	Point2f new_center(in_large.cols / 2, in_large.rows / 2);

	Mat rot_mat = getRotationMatrix2D(new_center, angle, 1);//得到旋转矩阵

	//将图形进行旋转
	Mat mat_rotated;
	warpAffine(in_large, mat_rotated, rot_mat, Size(in_large.cols, in_large.rows), CV_INTER_CUBIC);

	//得到最小外接图像
	Mat img_crop;
	getRectSubPix(mat_rotated, Size(rect_size.width, rect_size.height), new_center, img_crop);

	out = img_crop;

	return true;
}

//! 是否偏斜
//! 输入二值化图像，输出判断结果
bool isdeflection(const Mat& in, const double angle, double& slope)
{
	int nRows = in.rows;
	int nCols = in.cols;

	assert(in.channels() == 1);

	int comp_index[3];
	int len[3];

	comp_index[0] = nRows / 4;
	comp_index[1] = nRows / 4 * 2;
	comp_index[2] = nRows / 4 * 3;

	const uchar* p = NULL;

	for (int i = 0; i < 3; i++)
	{
		int index = comp_index[i];
		p = in.ptr<uchar>(index);

		int j = 0;
		int value = 0;
		while (0 == value && j < nCols)
			value = int(p[j++]);

		len[i] = j;
	}

	double maxlen = max(len[2], len[0]);
	double minlen = min(len[2], len[0]);
	double difflen = abs(len[2] - len[0]);

	double PI = 3.14159265;
	double g = tan(angle * PI / 180.0);

	if (maxlen - len[1] > nCols / 32 || len[1] - minlen > nCols / 32) {
		// 如果斜率为正，则底部在下，反之在上
		double slope_can_1 = double(len[2] - len[0]) / double(comp_index[1]);
		double slope_can_2 = double(len[1] - len[0]) / double(comp_index[0]);
		double slope_can_3 = double(len[2] - len[1]) / double(comp_index[0]);

		slope = abs(slope_can_1 - g) <= abs(slope_can_2 - g) ? slope_can_1 : slope_can_2;
		
		return true;
	}
	else {
		slope = 0;
	}

	return false;
}

//将水平斜矩形车牌，旋转成正矩形
void affine(const Mat& in, Mat& out, const double slope)
{
	Point2f dstTri[3];
	Point2f plTri[3];

	float height = (float)in.rows;
	float width = (float)in.cols;
	float xiff = (float)abs(slope) * height;

	if (slope > 0)
	{
		plTri[0] = Point2f(0, 0);
		plTri[1] = Point2f(width - xiff - 1, 0);
		plTri[2] = Point2f(xiff, height - 1);

		dstTri[0] = Point2f(xiff / 2, 0);
		dstTri[1] = Point2f(width - xiff / 2 - 1, 0);
		dstTri[2] = Point2f(xiff / 2, height - 1);
	}
	else
	{
		plTri[0] = Point2f(xiff / 2, 0);
		plTri[1] = Point2f(width - 1, 0);
		plTri[2] = Point2f(0, height - 1);

		dstTri[0] = Point2f(xiff / 2, 0);
		dstTri[1] = Point2f(width - xiff / 2 - 1, 0);
		dstTri[2] = Point2f(xiff / 2, height - 1);
	}

	Mat warp_mat = getAffineTransform(plTri, dstTri);//得到仿射矩阵
	Mat affine_mat;
	warpAffine(in, affine_mat, warp_mat, in.size(), CV_INTER_CUBIC);

	out = affine_mat;
}

void globelRotation(const Mat src, vector<Mat> &results, vector<vector<Point> > contours)
{
	vector<vector<Point> >::iterator itc = contours.begin();
	while (itc != contours.end())
	{
		double area = contourArea(*itc);//获取外部轮廓包围的面积
		RotatedRect mr = minAreaRect(Mat(*itc));//获取外部轮廓的最小外接矩形
		float angle = mr.angle;
		Size rect_size = mr.size;

		/*
		根据提取的车牌面积测量，车牌面积在3000~50000范围
		*/
		if (area > 3000 && area < 50000)
		{
			/*
			由于蓝牌白字、黄牌黑字的车牌规格为440mmx140mm，
			长宽比为3.14，所以将r设置为2.2~3.8范围
			*/
			float r = (float)mr.size.width / (float)mr.size.height;
			if (r < 1)
			{
				r = (float)mr.size.height / (float)mr.size.width;
				angle = 90 + angle;
				swap(rect_size.width, rect_size.height);
			}
			if (r > 2.2 && r < 3.8 && angle - 60 < 0 && angle + 60 > 0)
			{
				//用于计算二维旋转变换矩阵
				Mat rotmat = getRotationMatrix2D(mr.center, angle, 1);
				//对图像进行仿射变换
				Mat img_rotated;
				warpAffine(src, img_rotated, rotmat, src.size(), CV_INTER_CUBIC);

				/*
				由于getRectSubPix从原图中只能提取水平放和竖着放的矩形，
				且有的车牌是斜的，所以图像要经过上面的仿射变换，将车牌旋转正，
				才能提取完整的车牌，如果不将车牌旋转正，提取不到完整的车牌
				*/
				Mat resultMat;
				getRectSubPix(img_rotated, rect_size, mr.center, resultMat);
				results.push_back(resultMat);

			}
		}
		++itc;
	}
}



bool calcSafeRect(const RotatedRect& roi_rect, const Mat& src,
	Rect_<float>& safeBoundRect)
{
	Rect_<float> boudRect = roi_rect.boundingRect();//获取roi_rect的正矩形
	float t1_x = boudRect.x > 0 ? boudRect.x : 0;
	float t1_y = boudRect.y > 0 ? boudRect.y : 0;

	float br_x = boudRect.x + boudRect.width < src.cols ? boudRect.x + boudRect.width - 1 : src.cols - 1;
	float br_y = boudRect.y + boudRect.height < src.rows ? boudRect.y + boudRect.height : src.rows - 1;

	float roi_width = br_x - t1_x;
	float roi_height = br_y - t1_y;

	if (roi_width <= 0 || roi_height <= 0)
		return false;

	safeBoundRect = Rect_<float>(t1_x, t1_y, roi_width, roi_height);
	return true;
}

void partialRotation(const Mat src, Mat src_grey, vector<Mat> &results, vector<vector<Point> > contours)
{
	vector<RotatedRect> min_area_rects;//保存最小外接斜矩形

	vector<vector<Point> >::iterator itc = contours.begin();
	while (itc != contours.end())
	{

		double area = contourArea(*itc);//获取外部轮廓包围的面积
		RotatedRect mr = minAreaRect(Mat(*itc));//获取外部轮廓的最小外接矩形
		float angle = mr.angle;
		Size rect_size = mr.size;

		/*
		根据提取的车牌面积测量，车牌面积在3000~50000范围
		*/
		if (area > 3000 && area < 50000)
		{
			/*
			由于蓝牌白字、黄牌黑字的车牌规格为440mmx140mm，
			长宽比为3.14，所以将r设置为2.2~3.8范围
			*/
			float r = (float)mr.size.width / (float)mr.size.height;
			if (r < 1)
			{
				r = (float)mr.size.height / (float)mr.size.width;
				angle = 90 + angle;
			}
			if (r > 2.2 && r < 3.8 && angle - 60 < 0 && angle + 60 > 0)
			{
				min_area_rects.push_back(mr);
			}
		}
		++itc;
	}

	for (size_t i = 0; i < min_area_rects.size(); ++i)
	{
		RotatedRect roi_rect = min_area_rects[i];

		float r = (float)roi_rect.size.width / (float)roi_rect.size.height;
		float roi_angle = roi_rect.angle;

		Size roi_rect_size = roi_rect.size;
		if (r < 1)
		{
			roi_angle = 90 + roi_angle;
			swap(roi_rect_size.width, roi_rect_size.height);
		}
		Rect_<float> safeBoundRect;//保存最小外接正矩形的坐标
		bool isFormRect = calcSafeRect(roi_rect, src, safeBoundRect);
		if (!isFormRect)
			continue;
		Mat bound_mat = src(safeBoundRect);//在原图上截取车牌的最小正矩形图像
		Mat bound_mat_grey = src_grey(safeBoundRect);//在二值化图上截取车牌的最小正矩形图形

		Point2f roi_ref_center = roi_rect.center - safeBoundRect.tl();

		Mat deskew_mat;
		if (roi_angle - 5 < 0 && roi_angle + 5 > 0)
			deskew_mat = bound_mat;
		else
		{
			Mat rotated_mat;
			Mat rotated_mat_grey;

			//将车牌旋转成水平，旋转后的车牌不一定是正的，也有可能是斜矩形
			if (!rotation(bound_mat, rotated_mat, roi_rect_size, roi_ref_center, roi_angle))//旋转原图车牌
				continue;

			if (!rotation(bound_mat_grey, rotated_mat_grey, roi_rect_size, roi_ref_center, roi_angle))//旋转二值化图车牌
				continue;

			double roi_slope = 0;//保存斜水平车牌的斜率
			if (isdeflection(rotated_mat_grey, roi_angle, roi_slope))
			{
				affine(rotated_mat, deskew_mat, roi_slope);
			}
			else
				deskew_mat = rotated_mat;

		}

		results.push_back(deskew_mat);
	}

}

void plateColorLocate(const Mat src, vector<Mat> &results, const Color color)
{
	const float max_sv = 255;
	const float min_sv = 95;

	//blue的H范围
	const int min_blue = 100;  //100
	const int max_blue = 140;  //140

							   //yellow的H范围
	const int min_yellow = 15; //15
	const int max_yellow = 40; //40

							   //高斯模糊。Size中的数字影响车牌定位的效果。
	Mat src_blur;
	GaussianBlur(src, src_blur, Size(5, 5), 0, 0, BORDER_DEFAULT);

	Mat src_hsv;
	// 转到HSV空间进行处理，颜色搜索主要使用的是H分量进行蓝色与黄色的匹配工作
	cvtColor(src_blur, src_hsv, CV_BGR2HSV);

	//匹配模板基色,切换以查找想要的基色
	int min_h = 0;
	int max_h = 0;
	switch (color)
	{
	case BLUE:
		min_h = min_blue;
		max_h = max_blue;
		break;
	case YELLOW:
		min_h = min_yellow;
		max_h = max_yellow;
		break;
	}

	int channels = src_hsv.channels();
	int nRows = src_hsv.rows;
	//图像数据列需要考虑通道数的影响；
	int nCols = src_hsv.cols * channels;

	if (src_hsv.isContinuous())//连续存储的数据，按一行处理
	{
		nCols *= nRows;
		nRows = 1;
	}

	int i, j;
	uchar* p = NULL;

	for (i = 0; i < nRows; ++i)
	{
		p = src_hsv.ptr<uchar>(i);
		for (j = 0; j < nCols; j += 3)
		{
			int H = int(p[j]); //0-180
			int S = int(p[j + 1]);  //0-255
			int V = int(p[j + 2]);  //0-255

			if ((H > min_h && H < max_h) && (S > min_sv && S < max_sv) && (V > min_sv && V < max_sv))
			{
				p[j] = 0;
				p[j + 1] = 0;
				p[j + 2] = 255;
			}
			else
			{
				p[j] = 0;
				p[j + 1] = 0;
				p[j + 2] = 0;
			}
		}
	}
	// 获取颜色匹配后的二值灰度图
	Mat src_grey;
	cvtColor(src_hsv, src_grey, CV_RGB2GRAY);
	threshold(src_grey, src_grey, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

	//vector<Mat> hsvSplit_done;
	//split(src_hsv, hsvSplit_done);
	//src_grey = hsvSplit_done[2];

	//对图像进行闭操作
	Mat element = getStructuringElement(MORPH_RECT, Size(17, 3));
	morphologyEx(src_grey, src_grey, MORPH_CLOSE, element);

	// 提取外部轮廓
	vector<vector<Point> > contours;
	findContours(src_grey, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

	//globelRotation(src, results, contours);
	partialRotation(src, src_grey, results, contours);
}

void plateSobelLocate(Mat src, vector<Mat> &results)
{
	Mat src_blur;
	//高斯模糊处理
	GaussianBlur(src, src_blur, Size(5, 5), 0, 0, BORDER_DEFAULT);

	Mat src_gray;
	//灰度化处理
	if (src_blur.channels() == 3)
		cvtColor(src_blur, src_gray, CV_RGB2GRAY);
	else
		src_gray = src_blur;

	//sobel算子处理
	Mat grad_x, grad_y;
	Mat abs_grad_x, abs_grad_y;

	Sobel(src_gray, grad_x, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
	convertScaleAbs(grad_x, abs_grad_x);

	Mat grad;
	addWeighted(abs_grad_x, 1, 0, 0, 0, grad);

	//二值化处理
	Mat src_threshold;
	threshold(grad, src_threshold, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

	//闭操作
	Mat element = getStructuringElement(MORPH_RECT, Size(17, 3));
	morphologyEx(src_threshold, src_threshold, MORPH_CLOSE, element);

	//取外部轮廓
	vector<vector<Point> > contours;
	findContours(src_threshold, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

	//globelRotation(src, results, contours);
	partialRotation(src, src_threshold, results, contours);
}

void plateLocate(Mat src, vector<Mat> &results)
{
	vector<Mat> color_result;
	vector<Mat> sobel_result;
	plateColorLocate(src, results, BLUE);
	plateColorLocate(src, results, YELLOW);
	plateSobelLocate(src, results);
	
}