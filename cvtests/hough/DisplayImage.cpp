/* This is a standalone program. Pass an image name as the first parameter
of the program.  Switch between standard and probabilistic Hough transform
by changing "#if 1" to "#if 0" and back */
#include <cv.h>
#include <highgui.h>
#include <math.h>

using namespace cv;

int main(int argc, char** argv)
{
    Mat src, dst, color_dst;
    if( argc != 2 || !(src=imread(argv[1], 0)).data)
        return -1;

    Canny( src, dst, 400, 500, 3 );
    cvtColor( dst, color_dst, CV_GRAY2BGR );

#if 0
    vector<Vec2f> lines;
    HoughLines( dst, lines, 1, CV_PI/180, 100 );

    for( size_t i = 0; i < lines.size(); i++ )
    {
        float rho = lines[i][0];
        float theta = lines[i][1];
        double a = cos(theta), b = sin(theta);
        double x0 = a*rho, y0 = b*rho;
        Point pt1(cvRound(x0 + 1000*(-b)),
                  cvRound(y0 + 1000*(a)));
        Point pt2(cvRound(x0 - 1000*(-b)),
                  cvRound(y0 - 1000*(a)));
        line( color_dst, pt1, pt2, Scalar(0,0,255), 3, 8 );
    }
#else
    vector<Vec4i> lines;
    HoughLinesP( dst, lines, 1, CV_PI/180, 80, 30, 10 );
    // my code:
    int longestI = -1;
    int longestI2 = -1;
    float longest = 0;
    float longest2 = 0;
    for( size_t i = 0; i < lines.size(); i++ )
    {
        
        line( color_dst, Point(lines[i][0], lines[i][1]),
            Point(lines[i][2], lines[i][3]), Scalar(0,0,255), 3, 8 );
        float diffX = lines[i][0]-lines[i][2];
        float diffY = lines[i][1]-lines[i][3];
        float dist = (float)sqrt(diffX*diffX+diffY*diffY);
        if (diffX == 0)// don't want zero div shit
          continue;
        float slope = diffY/diffX;
        if (dist > longest || longestI == -1)
        {
          longestI = i;
          longest = dist;
        }else
        if (dist > longest2 || longestI2 == -1)
        {
          longestI2 = i;
          longest2 = dist;
        }
        //printf("start:(%i, %i), end:(%i, %i)\n", lines[i][0], lines[i][1], lines[i][2], lines[i][3]);
        //break;
    }
    {
      int i = longestI;
        line( color_dst, Point(lines[i][0], lines[i][1]),
            Point(lines[i][2], lines[i][3]), Scalar(0,0,255), 3, 8 );
      i = longestI2;
        line( color_dst, Point(lines[i][0], lines[i][1]),
            Point(lines[i][2], lines[i][3]), Scalar(0,0,255), 3, 8 );
    }
#endif
    namedWindow( "Source", 1 );
    imshow( "Source", src );

    waitKey(0);
    namedWindow( "Detected Lines", 1 );
    imshow( "Detected Lines", color_dst );

    waitKey(0);
    return 0;
}
