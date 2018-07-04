#include "plate.h"

int main()
{
	char imageName[20] = "./image/1.jpg";
	Color color = BLUE;
	Mat src = imread(imageName);
	int count = 1;
	int imageCnt = 1;
	while (!src.empty())
	{
		vector<Mat> results;
		plateLocate(src, results);
		for (int i = 0; i < (int)results.size(); ++i)
		{
			string str = "./result_image/";
			str = str + to_string(count) + ".jpg";
			imwrite(str, results[i]);
			++count;
		}
		++imageCnt;
		sprintf(imageName, "./image/%d.jpg", imageCnt);		
		src = imread(imageName);
	}
	return 0;
}
