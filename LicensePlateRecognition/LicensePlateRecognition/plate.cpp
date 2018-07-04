#include "plate.h"

bool rotation(Mat &in, Mat &out, const Size rect_size, const Point2f center, const double angle)
{
	Mat in_large;
	in_large.create(in.cols * 1.5, in.rows * 1.5, in.type());

	int x = in_large.cols / 2 - center.x > 0 ? in_large.cols / 2 - center.x : 0;//ʹ������������һ��
	int y = in_large.rows / 2 - center.y > 0 ? in_large.rows / 2 - center.y : 0;//��������������һ��

	int width = x + in.cols < in_large.cols ? in.cols : in_large.cols - x;//������ڣ�˵��in_larger�ߴ��С��û���ں�
	int height = y + in.rows < in_large.rows ? in.rows : in_large.rows - y;

	//����addWeighted������Ҫ���ںϵĳߴ������ȣ��������ȣ��ںϷ�����������Ҫ�ж�һ��
	if (width != in.cols || height != in.rows)
		return false;

	//�ں�ͼ�Σ���ͼ�λ�������in_large
	Mat imageRoi = in_large(Rect(x, y, width, height));
	addWeighted(imageRoi, 0, in, 1, 0, imageRoi);

	Point2f center_diff(in.cols / 2, in.rows / 2);
	Point2f new_center(in_large.cols / 2, in_large.rows / 2);

	Mat rot_mat = getRotationMatrix2D(new_center, angle, 1);//�õ���ת����

	//��ͼ�ν�����ת
	Mat mat_rotated;
	warpAffine(in_large, mat_rotated, rot_mat, Size(in_large.cols, in_large.rows), CV_INTER_CUBIC);

	//�õ���С���ͼ��
	Mat img_crop;
	getRectSubPix(mat_rotated, Size(rect_size.width, rect_size.height), new_center, img_crop);

	out = img_crop;

	return true;
}

//! �Ƿ�ƫб
//! �����ֵ��ͼ������жϽ��
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
		// ���б��Ϊ������ײ����£���֮����
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

//��ˮƽб���γ��ƣ���ת��������
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

	Mat warp_mat = getAffineTransform(plTri, dstTri);//�õ��������
	Mat affine_mat;
	warpAffine(in, affine_mat, warp_mat, in.size(), CV_INTER_CUBIC);

	out = affine_mat;
}

void globelRotation(const Mat src, vector<Mat> &results, vector<vector<Point> > contours)
{
	vector<vector<Point> >::iterator itc = contours.begin();
	while (itc != contours.end())
	{
		double area = contourArea(*itc);//��ȡ�ⲿ������Χ�����
		RotatedRect mr = minAreaRect(Mat(*itc));//��ȡ�ⲿ��������С��Ӿ���
		float angle = mr.angle;
		Size rect_size = mr.size;

		/*
		������ȡ�ĳ���������������������3000~50000��Χ
		*/
		if (area > 3000 && area < 50000)
		{
			/*
			�������ư��֡����ƺ��ֵĳ��ƹ��Ϊ440mmx140mm��
			�����Ϊ3.14�����Խ�r����Ϊ2.2~3.8��Χ
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
				//���ڼ����ά��ת�任����
				Mat rotmat = getRotationMatrix2D(mr.center, angle, 1);
				//��ͼ����з���任
				Mat img_rotated;
				warpAffine(src, img_rotated, rotmat, src.size(), CV_INTER_CUBIC);

				/*
				����getRectSubPix��ԭͼ��ֻ����ȡˮƽ�ź����ŷŵľ��Σ�
				���еĳ�����б�ģ�����ͼ��Ҫ��������ķ���任����������ת����
				������ȡ�����ĳ��ƣ��������������ת������ȡ���������ĳ���
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
	Rect_<float> boudRect = roi_rect.boundingRect();//��ȡroi_rect��������
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
	vector<RotatedRect> min_area_rects;//������С���б����

	vector<vector<Point> >::iterator itc = contours.begin();
	while (itc != contours.end())
	{

		double area = contourArea(*itc);//��ȡ�ⲿ������Χ�����
		RotatedRect mr = minAreaRect(Mat(*itc));//��ȡ�ⲿ��������С��Ӿ���
		float angle = mr.angle;
		Size rect_size = mr.size;

		/*
		������ȡ�ĳ���������������������3000~50000��Χ
		*/
		if (area > 3000 && area < 50000)
		{
			/*
			�������ư��֡����ƺ��ֵĳ��ƹ��Ϊ440mmx140mm��
			�����Ϊ3.14�����Խ�r����Ϊ2.2~3.8��Χ
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
		Rect_<float> safeBoundRect;//������С��������ε�����
		bool isFormRect = calcSafeRect(roi_rect, src, safeBoundRect);
		if (!isFormRect)
			continue;
		Mat bound_mat = src(safeBoundRect);//��ԭͼ�Ͻ�ȡ���Ƶ���С������ͼ��
		Mat bound_mat_grey = src_grey(safeBoundRect);//�ڶ�ֵ��ͼ�Ͻ�ȡ���Ƶ���С������ͼ��

		Point2f roi_ref_center = roi_rect.center - safeBoundRect.tl();

		Mat deskew_mat;
		if (roi_angle - 5 < 0 && roi_angle + 5 > 0)
			deskew_mat = bound_mat;
		else
		{
			Mat rotated_mat;
			Mat rotated_mat_grey;

			//��������ת��ˮƽ����ת��ĳ��Ʋ�һ�������ģ�Ҳ�п�����б����
			if (!rotation(bound_mat, rotated_mat, roi_rect_size, roi_ref_center, roi_angle))//��תԭͼ����
				continue;

			if (!rotation(bound_mat_grey, rotated_mat_grey, roi_rect_size, roi_ref_center, roi_angle))//��ת��ֵ��ͼ����
				continue;

			double roi_slope = 0;//����бˮƽ���Ƶ�б��
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

	//blue��H��Χ
	const int min_blue = 100;  //100
	const int max_blue = 140;  //140

							   //yellow��H��Χ
	const int min_yellow = 15; //15
	const int max_yellow = 40; //40

							   //��˹ģ����Size�е�����Ӱ�쳵�ƶ�λ��Ч����
	Mat src_blur;
	GaussianBlur(src, src_blur, Size(5, 5), 0, 0, BORDER_DEFAULT);

	Mat src_hsv;
	// ת��HSV�ռ���д�����ɫ������Ҫʹ�õ���H����������ɫ���ɫ��ƥ�乤��
	cvtColor(src_blur, src_hsv, CV_BGR2HSV);

	//ƥ��ģ���ɫ,�л��Բ�����Ҫ�Ļ�ɫ
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
	//ͼ����������Ҫ����ͨ������Ӱ�죻
	int nCols = src_hsv.cols * channels;

	if (src_hsv.isContinuous())//�����洢�����ݣ���һ�д���
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
	// ��ȡ��ɫƥ���Ķ�ֵ�Ҷ�ͼ
	Mat src_grey;
	cvtColor(src_hsv, src_grey, CV_RGB2GRAY);
	threshold(src_grey, src_grey, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

	//vector<Mat> hsvSplit_done;
	//split(src_hsv, hsvSplit_done);
	//src_grey = hsvSplit_done[2];

	//��ͼ����бղ���
	Mat element = getStructuringElement(MORPH_RECT, Size(17, 3));
	morphologyEx(src_grey, src_grey, MORPH_CLOSE, element);

	// ��ȡ�ⲿ����
	vector<vector<Point> > contours;
	findContours(src_grey, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

	//globelRotation(src, results, contours);
	partialRotation(src, src_grey, results, contours);
}

void plateSobelLocate(Mat src, vector<Mat> &results)
{
	Mat src_blur;
	//��˹ģ������
	GaussianBlur(src, src_blur, Size(5, 5), 0, 0, BORDER_DEFAULT);

	Mat src_gray;
	//�ҶȻ�����
	if (src_blur.channels() == 3)
		cvtColor(src_blur, src_gray, CV_RGB2GRAY);
	else
		src_gray = src_blur;

	//sobel���Ӵ���
	Mat grad_x, grad_y;
	Mat abs_grad_x, abs_grad_y;

	Sobel(src_gray, grad_x, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
	convertScaleAbs(grad_x, abs_grad_x);

	Mat grad;
	addWeighted(abs_grad_x, 1, 0, 0, 0, grad);

	//��ֵ������
	Mat src_threshold;
	threshold(grad, src_threshold, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

	//�ղ���
	Mat element = getStructuringElement(MORPH_RECT, Size(17, 3));
	morphologyEx(src_threshold, src_threshold, MORPH_CLOSE, element);

	//ȡ�ⲿ����
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